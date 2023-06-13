
#include "VoltageRegulator.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"

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
        this->timerStateCheck.async_wait([&](const boost::system::error_code& err) {
            if (err != boost::asio::error::operation_aborted)
            {
                this->ConfirmStatusAfterTimeout();
            }
        });

        // This might glitch if regulator needs some time to disable
        this->enabled->SetLevel(this->stateShadow == ENABLED);
    }
}

vector<Signal*> VoltageRegulator::Signals(void)
{
    vector<Signal*> vec;

    vec.push_back(this->enabled);
    vec.push_back(this->powergood);
    vec.push_back(this->fault);

    return vec;
}

void VoltageRegulator::Update(void)
{
    this->newLevel = this->in->GetLevel();
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
            this->timerStateCheck.expires_from_now(boost::posix_time::microseconds(this->stateChangeTimeoutUsec / this->retries));
            this->timerStateCheck.async_wait([&](const boost::system::error_code& err) {
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
        // Something is wrong. Turn off regulator.
        this->control.SetState(DISABLED);
    }
}

void VoltageRegulator::ApplyStatus(enum RegulatorStatus status)
{
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
    // If status haven't changed just return
    // This can happen if a PRE_DISABLE or PRE_VOLTAGE_CHANGE is sent
    if (this->statusShadow == status)
        return;

    if (status == NOCHANGE)
        return;

    log_debug(this->name + ": status changed to " +  this->control.StatusToString(status) + ", was " +  this->control.StatusToString(this->statusShadow));
    if (this->statusShadow == ERROR && status != ERROR) {
        log_err(this->name + ": Regulator recovered from error, new state is: " +  this->control.StatusToString(status));
        log_sel(this->name + ": Regulator recovered from error, new state is: " +  this->control.StatusToString(status),
                "/xyz/openbmc_project/inventory/system/chassis/motherboard", false);
    }
    this->statusShadow = status;

    if (status == OFF)
    {
        this->enabled->SetLevel(false);
        this->powergood->SetLevel(false);
        this->fault->SetLevel(false);
    }
    else if (status == ERROR)
    {
        // Errors might get cleared by interrupt handlers before we can
        // read them...
        this->enabled->SetLevel(true);
        this->powergood->SetLevel(false);
        this->fault->SetLevel(true);
    }
    else if (status == ON)
    {
        this->enabled->SetLevel(true);
        this->powergood->SetLevel(true);
        this->fault->SetLevel(false);
    }
    if (this->pendingLevelChange) {
        if (this->pendingNewLevel == ENABLED && status == ON)
            this->pendingLevelChange = false;
        else if (this->pendingNewLevel == DISABLED && status == OFF)
            this->pendingLevelChange = false;
        else {
            log_err(this->name + ": Got unexpected regulator status! Requested state " +
                 this->control.StateToString(this->pendingNewLevel) + ", got status " +  this->control.StatusToString(status));
        }
    } else {
        log_err(this->name + ": Got unexpected regulator status! Requested no state change, got status " +  this->control.StatusToString(status));
        log_sel(this->name + ": Got unexpected regulator status! Requested no state change, got status: " +  this->control.StatusToString(status),
                "/xyz/openbmc_project/inventory/system/chassis/motherboard", true);
        // Attempting to change it back is pointless.
        // The power sequencing logic will likely shutdown the system anyways.
    }
}

void VoltageRegulator::Poll(void)
{
    // Regulators do not propagate their status changes through IRQs.
    // Only errors are passed through state change....
    this->timerPoll.expires_from_now(boost::posix_time::seconds(1));
    this->timerPoll.async_wait([&](const boost::system::error_code& err) {
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

VoltageRegulator::VoltageRegulator(boost::asio::io_context& io,
                                   struct ConfigRegulator* cfg,
                                   SignalProvider& prov, string root) :
    statusShadow(NOCHANGE), name(cfg->Name) , stateChangeTimeoutUsec(cfg->TimeoutUsec), timerStateCheck(io),
    pendingLevelChange(false), pendingNewLevel(DISABLED), timerPoll(io), control(io,  cfg, root)
{
    this->in = prov.FindOrAdd(cfg->Name + "_On");
    this->in->AddReceiver(this);

    this->enabled = prov.FindOrAdd(cfg->Name + "_Enabled");
    this->fault = prov.FindOrAdd(cfg->Name + "_Fault");
    this->powergood = prov.FindOrAdd(cfg->Name + "_PowerGood");

    if (this->stateChangeTimeoutUsec == 0)
       this->stateChangeTimeoutUsec = DEFAULT_TIMEOUT_USEC;

    // Set initial signal levels
    this->stateShadow =  this->control.DecodeState();

    // Silence warning when calling ApplyStatus
    this->pendingLevelChange = true;
    if (this->control.DecodeStatus() == OFF)
       this->pendingNewLevel = DISABLED;
    else if (this->control.DecodeStatus() == ON)
       this->pendingNewLevel = ENABLED;

    // Set current state
    this->ApplyStatus(this->control.DecodeStatus());

    // Register sysfs watcher
     this->control.RegisterStatusCallback([&](const enum RegulatorStatus status) {
        if (this->statusShadow == status)
        {
            io.post([&]() {
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
    });
     this->control.RegisterStateCallback([&](const enum RegulatorState state) {
        this->enabled->SetLevel(state == ENABLED);
    });
     this->control.RegisterEventCallback([&](const unsigned long events) {
        enum RegulatorStatus status;
        status =  this->control.DecodeEvents(events);
        if (status == ON) {
            this->enabled->SetLevel(true);
        } else if (status == OFF) {
            this->enabled->SetLevel(false);
        } else {
            this->ApplyStatus(status);
        }
    });
}

VoltageRegulator::~VoltageRegulator()
{}
