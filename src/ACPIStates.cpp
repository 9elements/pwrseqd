

#include "ACPIStates.hpp"

#include "Config.hpp"
#include "Logging.hpp"
#include "Signal.hpp"

using namespace std;

/*
 * xyz.openbmc_project.State.Host.CurrentHostState    : Off
 * xyz.openbmc_project.State.Chassis.CurrentPowerState: Off
 * PSU                                                : Off
 * DRAM powered                                       : false
 * Platform reports off                               : -
 * ACPI State                                         : G3
 * Signal STATE_REQ_CHASSIS_ON                        : false
 * Signal STATE_REQ_HOST_ON                           : false
 *
 * xyz.openbmc_project.State.Host.CurrentHostState    : Off
 * xyz.openbmc_project.State.Chassis.CurrentPowerState: On
 * PSU                                                : On
 * DRAM powered                                       : false
 * Platform reports off                               : true
 * ACPI State                                         : S5/S4
 * Signal STATE_REQ_CHASSIS_ON                        : true
 * Signal STATE_REQ_HOST_ON                           : false
 *
 * xyz.openbmc_project.State.Host.CurrentHostState    : Standby
 * xyz.openbmc_project.State.Chassis.CurrentPowerState: On
 * PSU                                                : On
 * DRAM powered                                       : true
 * Platform reports off                               : true
 * ACPI State                                         : S3
 * Signal STATE_REQ_CHASSIS_ON                        : true
 * Signal STATE_REQ_HOST_ON                           : false
 *
 * xyz.openbmc_project.State.Host.CurrentHostState    : On
 * xyz.openbmc_project.State.Chassis.CurrentPowerState: On
 * PSU                                                : On
 * DRAM powered                                       : true
 * Platform reports on                                : true
 * ACPI State                                         : S0
 * Signal STATE_REQ_CHASSIS_ON                        : true
 * Signal STATE_REQ_HOST_ON                           : true
 */

static const struct
{
    enum ACPILevel l;
    string signal;
    string name;
} ObservedStates[4] = {{
                           .l = ACPI_G3,
                           .signal = "ACPI_STATE_IS_G3",
                           .name = "G3",
                       },
                       {
                           .l = ACPI_S5,
                           .signal = "ACPI_STATE_IS_S5",
                           .name = "S5",
                       },
                       {
                           .l = ACPI_S3,
                           .signal = "ACPI_STATE_IS_S3",
                           .name = "S3",
                       },
                       {
                           .l = ACPI_S0,
                           .signal = "ACPI_STATE_IS_S0",
                           .name = "S0",
                       }};

void ACPIStates::RequestHost(const bool powerOn)
{
    this->signalHostState->SetLevel(powerOn);

    this->Update();
}

void ACPIStates::RequestChassis(const bool powerOn)
{
    this->signalChassisState->SetLevel(powerOn);

    this->Update();
}

// GetCurrent returns the current ACPI state.
enum ACPILevel ACPIStates::GetCurrent(void)
{
    int levels = 0;
    for (auto it : this->outputs)
    {
        if (it.second->GetLevel())
            levels++;

        string l = "on";
        if (!it.second->GetLevel())
            l = "off";
        log_debug("ACPI State " + it.second->Name() + " is " + l);
    }
    if (levels > 1)
    {
        log_err("Logic error: Multiple ACPI states active at the same time");
        return ACPI_INVALID;
    }

    for (auto it : this->outputs)
    {
        if (it.second->GetLevel())
            return it.first;
    }

    return ACPI_INVALID;
}

void ACPIStates::Update(void)
{
    enum ACPILevel newLevel = this->GetCurrent();
    switch (newLevel)
    {
        case ACPI_S0:
            this->idleTimer.cancel();
            if (this->signalHostState->GetLevel())
                this->dbus->SetHostState(dbus::HostState::running);
            else
                this->dbus->SetHostState(dbus::HostState::transitionToOff);
            this->dbus->SetChassisState(true);
            if (this->signalPostDone != nullptr) {
                if (this->signalPostDone->GetLevel()) {
                    this->dbus->SetBootState(dbus::BootProgress::SystemInitComplete);
                    this->dbus->SetOSState(dbus::OSState::standby);
                } else {
                    this->dbus->SetBootState(dbus::BootProgress::PrimaryProcInit);
                    this->dbus->SetOSState(dbus::OSState::inactive);
                }
            } else {
                this->dbus->SetBootState(dbus::BootProgress::PrimaryProcInit);
                this->dbus->SetOSState(dbus::OSState::standby);
            }
            break;
        case ACPI_S3:
            this->idleTimer.cancel();
            if (this->signalHostState->GetLevel())
                this->dbus->SetHostState(dbus::HostState::transitionToRunning);
            else
                this->dbus->SetHostState(dbus::HostState::standby);
            this->dbus->SetChassisState(true);
            this->dbus->SetOSState(dbus::OSState::inactive);
            this->dbus->SetBootState(dbus::BootProgress::Unspecified);
            break;
        case ACPI_S5:
            if (acpi_g3_on_host_shutdown &&
                this->lastLevel != ACPI_G3 && this->lastLevel != ACPI_S5) {
                this->idleTimer.expires_from_now(boost::posix_time::seconds(30));
                this->idleTimer.async_wait([&](const boost::system::error_code& err) {
                    if (err != boost::asio::error::operation_aborted)
                    {
                        if (this->GetCurrent() == ACPI_S5)
                        {
                            this->dbus->RequestedPowerTransition(dbus::ChassisTransition::off);
                            log_info("Turning off chassis due to idle host");
                        }
                    }
                });
            }
            if (this->signalHostState->GetLevel())
                this->dbus->SetHostState(dbus::HostState::transitionToRunning);
            else
                this->dbus->SetHostState(dbus::HostState::off);
            this->dbus->SetChassisState(true);
            this->dbus->SetOSState(dbus::OSState::inactive);
            this->dbus->SetBootState(dbus::BootProgress::Unspecified);
            break;
        case ACPI_G3:
            this->idleTimer.cancel();
            this->powerCycleTimer.cancel();

            this->dbus->SetChassisState(false);
            this->dbus->SetHostState(dbus::HostState::off);
            this->dbus->SetOSState(dbus::OSState::inactive);
            this->dbus->SetBootState(dbus::BootProgress::Unspecified);
            break;
    }
    this->lastLevel = newLevel;
}

bool ACPIStates::RequestedHostTransition(const std::string& requested,
                                         std::string& resp)
{
    if (requested == dbus::getHostTransition(dbus::HostTransition::off))
    {
        this->RequestHost(false);
        // Leave chassis as is. Platform managment might even run in ACPI_S5.
    }
    else if (requested == dbus::getHostTransition(dbus::HostTransition::on))
    {
        this->RequestChassis(true);
        // Give statemachine some time to drive GPIOs before changing host state
        this->io->post([this] {
            this->io->post([this] {
                this->RequestHost(true);
            });
        });
    }
    else
    {
        log_err("Unrecognized host state transition request." + requested);
        throw std::invalid_argument("Unrecognized Transition Request");
        return false;
    }
    resp = requested;
    return true;
}

bool ACPIStates::RequestedPowerTransition(const std::string& requested,
                                          std::string& resp)
{

    if (requested == dbus::getChassisTransition(dbus::ChassisTransition::off))
    {
        this->RequestHost(false);
        // Give statemachine some time to drive GPIOs before changing chassis state
        this->io->post([this] {
            this->io->post([this] {
                this->RequestChassis(false);
            });
        });
    }
    else if (requested == dbus::getChassisTransition(dbus::ChassisTransition::on))
    {
        this->RequestChassis(true);
        // Leave host as is. It might turn on automatically.
    }
    else if (requested ==
             dbus::getChassisTransition(dbus::ChassisTransition::power_cycle))
    {
        if (this->GetCurrent() == ACPI_G3)
        {
            this->RequestChassis(true);
        }
        else
        {
            this->RequestHost(false);
            // Give statemachine some time to drive GPIOs before changing chassis state
            this->io->post([this] {
                this->io->post([this] {
                    this->RequestChassis(false);
                });
            });

            this->powerCycleTimer.expires_from_now(
                boost::posix_time::seconds(10));
            this->powerCycleTimer.async_wait([&](const boost::system::
                                                     error_code& err) {
                if (!err || err == boost::asio::error::operation_aborted)
                {
                    if (this->GetCurrent() != ACPI_G3)
                    {
                        log_err(
                            "Did not transition to state ACPI_G3 within timeout");
                    }
                    else
                    {
                        this->RequestChassis(true);
                        this->RequestHost(true);
                    }
                }
            });
        }
    }
    else
    {
        log_err("Unrecognized chassis state transition request." + requested);
        throw std::invalid_argument("Unrecognized Transition Request");

        return false;
    }
    resp = requested;
    return true;
}

ACPIStates::ACPIStates(Config& cfg, SignalProvider& sp,
                       boost::asio::io_service& io,
                       Dbus& d, bool auto_acpi_g3_on_shutdown) :
    io(&io), sp{&sp},
    dbus(&d), lastLevel{ACPI_G3}, powerCycleTimer(io), idleTimer(io),
    acpi_g3_on_host_shutdown(auto_acpi_g3_on_shutdown)
{

    for (auto it : ObservedStates)
    {
        Signal* s = this->sp->FindOrAdd(it.signal);
        this->outputs[it.l] = s;
        s->AddReceiver(this);
    }

    this->signalHostState = sp.FindOrAdd("STATE_REQ_HOST_ON");
    this->signalChassisState = sp.FindOrAdd("STATE_REQ_CHASSIS_ON");
    this->signalPostDone = sp.FindOrAdd("STATE_POST_DONE");
    if (this->signalPostDone == nullptr) {
        log_debug("Could not find signal STATE_POST_DONE. POST will be unreliable.");
    } else {
        this->signalPostDone->AddReceiver(this);
    }
    this->dbus->RegisterRequestedHostTransition(
        [this](const std::string& requested, std::string& resp) {
            log_debug("RequestedHostTransition to " + requested);
            return this->RequestedHostTransition(requested, resp);
        });
    this->dbus->RegisterRequestedPowerTransition(
        [this](const std::string& requested, std::string& resp) {
            log_debug("RequestedPowerTransition to " + requested);
            return this->RequestedPowerTransition(requested, resp);
        });
    this->dbus->Initialize();
}

ACPIStates::~ACPIStates()
{
    for (auto it : ObservedStates)
    {
        this->outputs.erase(it.l);
    }
}

vector<Signal*> ACPIStates::Signals()
{
    vector<Signal*> vec;

    vec.push_back(this->signalHostState);
    vec.push_back(this->signalChassisState);
    if (this->signalPostDone != nullptr) {
        vec.push_back(this->signalPostDone);
    }
    return vec;
}
