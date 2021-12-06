

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
    switch (this->GetCurrent())
    {
        case ACPI_S0:
            if (this->signalHostState->GetLevel())
                this->dbus.SetHostState(dbus::HostState::running);
            else
                this->dbus.SetHostState(dbus::HostState::transitionToOff);
            this->dbus.SetChassisState(true);
            break;
        case ACPI_S3:
            if (this->signalHostState->GetLevel())
                this->dbus.SetHostState(dbus::HostState::transitionToRunning);
            else
                this->dbus.SetHostState(dbus::HostState::standby);
            this->dbus.SetChassisState(true);
            break;
        case ACPI_S5:
            if (this->signalHostState->GetLevel())
                this->dbus.SetHostState(dbus::HostState::transitionToRunning);
            else
                this->dbus.SetHostState(dbus::HostState::off);
            this->dbus.SetChassisState(true);
            break;
        case ACPI_G3:
            this->powerCycleTimer.cancel();

            this->dbus.SetChassisState(false);
            this->dbus.SetHostState(dbus::HostState::off);
            break;
    }
}

bool ACPIStates::RequestedHostTransition(const std::string& requested,
                                         std::string& resp)
{
    if (requested == "xyz.openbmc_project.State.Host.Transition.Off")
    {
        this->RequestHost(false);
        // Leave chassis as is. Platform managment might even run in ACPI_S5.
    }
    else if (requested == "xyz.openbmc_project.State.Host.Transition.On")
    {
        this->RequestChassis(true);
        this->RequestHost(true);
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

    if (requested == "xyz.openbmc_project.State.Chassis.Transition.Off")
    {
        this->RequestHost(false);
        this->RequestChassis(false);
    }
    else if (requested == "xyz.openbmc_project.State.Chassis.Transition.On")
    {
        this->RequestChassis(true);
        // Leave host as is. It might turn on automatically.
    }
    else if (requested ==
             "xyz.openbmc_project.State.Chassis.Transition.PowerCycle")
    {
        if (this->GetCurrent() == ACPI_G3)
        {
            this->RequestChassis(true);
        }
        else
        {
            this->RequestChassis(false);

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
                       boost::asio::io_service& io) :
    sp{&sp},
    dbus{cfg, io}, powerCycleTimer(io)
{

    for (auto it : ObservedStates)
    {
        Signal* s = this->sp->FindOrAdd(it.signal);
        this->outputs[it.l] = s;
        s->AddReceiver(this);
    }

    this->signalHostState = sp.FindOrAdd("STATE_REQ_HOST_ON");
    this->signalChassisState = sp.FindOrAdd("STATE_REQ_CHASSIS_ON");

    this->dbus.RegisterRequestedHostTransition(
        [this](const std::string& requested, std::string& resp) {
            log_debug("RequestedHostTransition to " + requested);
            return this->RequestedHostTransition(requested, resp);
        });
    this->dbus.RegisterRequestedPowerTransition(
        [this](const std::string& requested, std::string& resp) {
            log_debug("RequestedPowerTransition to " + requested);
            return this->RequestedPowerTransition(requested, resp);
        });
    this->dbus.Initialize();
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

    return vec;
}
