
#include "VoltageRegulator.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"

#include <boost/thread/lock_guard.hpp>

#include <filesystem>
#include <fstream>

using namespace std;
using namespace std::filesystem;


// Name returns the instance name
string VoltageRegulator::Name(void)
{
    return this->name;
}

void VoltageRegulator::Apply(void)
{
    const enum RegulatorState s = this->newLevel ? ENABLED : DISABLED;

    if (s != this->stateShadow)
    {
        this->timerStateCheck.cancel();

        this->control.SetState(s);

        // Setting the state might fail. Read current value from consumer.
        this->stateShadow = this->control.DecodeState();
        if (s != this->stateShadow)
        {
            // Try again.
            this->control.SetState(s);
            this->stateShadow = this->control.DecodeState();
        }

        this->pendingLevelChange = true;
        this->pendingNewLevel = s;
        this->retries = 10;
        this->timerStateCheck.expires_from_now(boost::posix_time::microseconds(this->stateChangeTimeoutUsec / this->retries));
        this->timerStateCheck.async_wait([this](const boost::system::error_code& err) {
            if (err != boost::asio::error::operation_aborted)
            {
                this->ConfirmStatusAfterTimeout();
            }
        });
        bool enabled = this->stateShadow == ENABLED;
        io->post([this, enabled]() {
            // This might glitch if regulator needs some time to disable
            this->enabled->SetLevel(enabled);
        });
    }
}

vector<Signal*> VoltageRegulator::Signals(void)
{
    vector<Signal*> vec;

    vec.push_back(this->enabled);
    vec.push_back(this->powergood);
    vec.push_back(this->fault);
    vec.push_back(this->dummy);

    return vec;
}

void VoltageRegulator::Update(void)
{
    this->newLevel = this->in->GetLevel();
    ioOutput->post([this]() {
        this->Apply();
    });
}

void VoltageRegulator::ConfirmStatusAfterTimeout(void)
{
    /* In case notify is too slow/fast */
    enum RegulatorStatus status = this->control.DecodeStatus();
    if (this->statusShadow != status) {
        log_debug(this->name + ": timerStateCheck read 'status': " +  this->control.StatusToString(status));
    }
    this->ApplyStatus(status);

    bool failure = false;
    // First check state. Should never missmatch with a single regulator consumer.
    if (this->pendingNewLevel == DISABLED && this->stateShadow == ENABLED) {
        failure = true;
    } else if (this->pendingNewLevel == ENABLED && this->stateShadow == DISABLED) {
        failure = true;
    }
    // Now check status
    if (this->pendingNewLevel == ENABLED && this->statusShadow != ON) {
        failure = true;
    } else if (this->pendingNewLevel == DISABLED && this->statusShadow != OFF) {
        failure = true;
    }

    if (failure) {
        if (this->retries > 0) {
            log_err(this->name + ": State didn't change yet to " + ((this->pendingNewLevel == ENABLED) ? "ON" : "OFF") + ", is " + this->control.StatusToString(status));
            this->timerStateCheck.expires_from_now(boost::posix_time::microseconds(this->stateChangeTimeoutUsec / this->retries));
            this->timerStateCheck.async_wait([this](const boost::system::error_code& err) {
                if (err != boost::asio::error::operation_aborted)
                {
                    this->ConfirmStatusAfterTimeout();
                }
            });
            this->retries--;

            return;
        }
        log_err(this->name + ": State didn't change after " + to_string(this->stateChangeTimeoutUsec * 2) + " usec.");

        this->pendingLevelChange = false;
        this->ApplyStatus(ERROR);
    }
}

void VoltageRegulator::ApplyStatus(enum RegulatorStatus status)
{
    boost::lock_guard<boost::mutex> guard(this->lock);

    // If status is undefined regulator is not in error state.
    // Assume it's on when state is set to enabled, else off.
    // TODO: Fix the linux driver to return proper status.
    if (status == UNDEFINED) {
      if (this->stateShadow == ENABLED) {
        status = ON;
      } else {
        status = OFF;
      }
    }

    if (status == NOCHANGE)
        return;

    // If status haven't changed just return
    // This can happen if a PRE_DISABLE or PRE_VOLTAGE_CHANGE is sent
    if (this->statusShadow == status)
        return;

    log_debug(this->name + ": status changed to " +  this->control.StatusToString(status) + ", was " + this->control.StatusToString(this->statusShadow));

    if (this->statusShadow == ERROR && status != ERROR) {
        log_err(this->name + ": Regulator recovered from error, new state is: " +  this->control.StatusToString(status));
        log_sel(this->name + ": Regulator recovered from error, new state is: " +  this->control.StatusToString(status),
                "/xyz/openbmc_project/inventory/system/chassis/motherboard", false);
    }

    if (this->pendingLevelChange) {
        if (this->pendingNewLevel == ENABLED && status == ON)
            this->pendingLevelChange = false;
        else if (this->pendingNewLevel == DISABLED && status == OFF)
            this->pendingLevelChange = false;
        else {
            log_err(this->name + ": Got unexpected regulator status! Requested state " +
                 this->control.StateToString(this->pendingNewLevel) + ", got status " +  this->control.StatusToString(status));
            if (status == ERROR) {
                // Status error is also seen on regulator turn on/off as the regulation is
                // out of range for a short moment.
                return;
            }
        }
    } else {
        if (status == ERROR && this->stateShadow == DISABLED) {
            // Regulator should be off, ignore errors as OUT OF REGULATION or UNDERVOLATE is unlikely,
            // but expected when regulator was turned off or is being turned off.
            return;
        }
        if (status == ERROR) {
            log_err(this->name + ": Got unexpected regulator status! Requested no state change, got status " + this->control.StatusToString(status));
            log_sel(this->name + ": Got unexpected regulator status! Requested no state change, got status: " + this->control.StatusToString(status),
                "/xyz/openbmc_project/inventory/system/chassis/motherboard", true);
        }
    }
    this->statusShadow = status;

    this->io->post([this, status]() {
        if (status == OFF)
        {
            this->enabled->SetLevel(false);
            this->powergood->SetLevel(false);
            this->fault->SetLevel(false);
        }
        else if (status == ERROR)
        {
            this->powergood->SetLevel(false);
            this->fault->SetLevel(true);
            if (this->errorCallback && !this->noErrorCallback) {
                this->errorCallback (this);
            }
            // The state is not updated here to prevent oscillations.
            // It's the kernel responsibility to turn off regulators with
            // fault interrupt handlers!
        }
        else if (status == ON)
        {
            this->enabled->SetLevel(true);
            this->powergood->SetLevel(true);
            this->fault->SetLevel(false);
        }
    });
}

void VoltageRegulator::Poll(void)
{
    // Regulators do not propagate their status changes through IRQs.
    // Only errors are passed through state change....
    this->timerPoll.expires_from_now(boost::posix_time::seconds(1));
    this->timerPoll.async_wait([this](const boost::system::error_code& err) {
        if (err != boost::asio::error::operation_aborted)
        {
            string statusText =  this->control.ReadStatus();
            enum RegulatorStatus status =  this->control.DecodeStatus(statusText);
            if (status == INVALID) {
                log_err(this->name + ": Got invalid status string '"+statusText+"'");
            }
            this->ApplyStatus(status);

            this->Poll();
        }
    });
}

void VoltageRegulator::CheckStatus(bool later)
{
    if (later) {
        this->io->post([this]() { this->CheckStatus(false); });
        return;
    }
    string statusText = this->control.ReadStatus();
    enum RegulatorStatus status = this->control.DecodeStatus(statusText);
    if (status == INVALID) {
        log_err(this->name + ": Got invalid status string '"+statusText+"'");
    }
    if (this->statusShadow == status)
    {
        this->io->post([this]() {
            string statusText = this->control.ReadStatus();
            enum RegulatorStatus status2 = this->control.DecodeStatus(statusText);
            if (status2 == INVALID) {
                log_err(this->name + ": Got invalid status string '"+statusText+"'");
            }
            this->ApplyStatus(status2);
        });
    } else {
        this->ApplyStatus(status);
    }
}

VoltageRegulator::VoltageRegulator(boost::asio::io_context& io,
                                   boost::asio::io_service& IoOutput,
                                   struct ConfigRegulator* cfg,
                                   SignalProvider& prov, string root,
                                   function<void(VoltageRegulator*)> const& lamda) :
    io(&io), ioOutput(&IoOutput), statusShadow(NOCHANGE), name(cfg->Name),
    stateChangeTimeoutUsec(cfg->TimeoutUsec),
    timerStateCheck(IoOutput), pendingLevelChange(false), pendingNewLevel(DISABLED), timerPoll(IoOutput),
    control(cfg, root), errorCallback(lamda), noErrorCallback(cfg->AllowFaulting)
{
    this->in = prov.FindOrAdd(cfg->Name + "_On");
    this->in->AddReceiver(this);

    this->enabled = prov.FindOrAdd(cfg->Name + "_Enabled");
    this->fault = prov.FindOrAdd(cfg->Name + "_Fault");
    this->powergood = prov.FindOrAdd(cfg->Name + "_PowerGood");
    this->dummy = prov.FindOrAdd(cfg->Name + "_IsDummy");
    this->dummy->SetLevel(false);

    if (this->stateChangeTimeoutUsec == 0)
       this->stateChangeTimeoutUsec = DEFAULT_TIMEOUT_USEC;

    // Set initial signal levels
    this->stateShadow = this->control.DecodeState();

    // Silence warning when calling ApplyStatus
    this->pendingLevelChange = true;
    if (this->control.DecodeStatus() == OFF)
       this->pendingNewLevel = DISABLED;
    else if (this->control.DecodeStatus() == ON)
       this->pendingNewLevel = ENABLED;

    // Sync internal status with the current regulator status
    this->ApplyStatus(this->control.DecodeStatus());

    netlink = GetNetlinkRegulatorEvents(IoOutput);
    if (netlink) {
        netlink->Register(this->name, [this](std::string name, uint64_t events) {
            log_debug(this->name + ": Got NETLINK event: " + to_string(events));
            if (events & REGULATOR_EVENT_FAILURE) {
                this->ApplyStatus(ERROR);
                log_sel(this->name + ": " + this->control.EventsToString(events), "", false);
            }
            if (events & REGULATOR_EVENT_EN_DIS)
            {
                // The enable/disable event indicates that the status will change soon,
                // but it doesn't allow to make assumptions about the actual status.
                //
                // Reschedule a status read check as the regulator is slow.
                // ConfirmStatusAfterTimeout does the same, but even slower.
                this->io->post([this, events]() {
                    // When the regulator indicated a failure at the same time
                    // give the statemachine some time to run the error handler
                    // before clearing the error flag inside the regulator.
                    if (events & REGULATOR_EVENT_FAILURE) {
                        this->CheckStatus(true);
                        this->io->post([this, events]() {
                            if ((events & REGULATOR_EVENT_EN_DIS) == REGULATOR_EVENT_ENABLE)
                            {
                                this->enabled->SetLevel(true);
                            }
                            else if ((events & REGULATOR_EVENT_EN_DIS) == REGULATOR_EVENT_DISABLE)
                            {
                                this->enabled->SetLevel(false);
                            }
                        });
                    } else {
                        this->CheckStatus(false);
                        if ((events & REGULATOR_EVENT_EN_DIS) == REGULATOR_EVENT_ENABLE)
                        {
                            this->enabled->SetLevel(true);
                        }
                        else if ((events & REGULATOR_EVENT_EN_DIS) == REGULATOR_EVENT_DISABLE)
                        {
                            this->enabled->SetLevel(false);
                        }
                    }
                });
            }
        });
    }
}

VoltageRegulator::~VoltageRegulator()
{
    if (netlink) {
        netlink->Unregister(this->name);
    }
}
