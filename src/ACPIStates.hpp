#pragma once

#include "Config.hpp"
#include "Dbus.hpp"
#include "IODriver.hpp"
#include "Signal.hpp"
#include "SignalProvider.hpp"

#include <boost/asio/deadline_timer.hpp>

#include <unordered_map>
#include <vector>

using namespace std;

enum ACPILevel
{
    ACPI_INVALID,
    ACPI_G3,
    ACPI_S5,
    ACPI_S3,
    ACPI_S0,
};

// The ACPIStates class handles the ACPI states
class ACPIStates : public SignalDriver, SignalReceiver
{
  public:
    // RequestChassis tells the logic which on is the desired power state of the
    // chassis. When Chassis is 'Off', the PSU is Off as well and the system is
    // in ACPI_G3 state
    void RequestChassis(const bool powerOn);
    // RequestHost tells the logic which on is the desired power state of the
    // host. When Host is 'Off', the system is in ACPI_S5, else in Standby or
    // On.
    void RequestHost(const bool powerOn);

    // GetCurrent returns the current ACPI state.
    enum ACPILevel GetCurrent(void);

    ACPIStates(Config& cfg,
               SignalProvider& sp,
               boost::asio::io_service& io,
               Dbus& d,
               bool auto_acpi_g3_on_shutdown);
    ~ACPIStates();

    vector<Signal*> Signals(void);

    void Update(void);

  private:
    bool RequestedPowerTransition(const std::string& requested,
                                  std::string& resp);
    bool RequestedHostTransition(const std::string& requested,
                                 std::string& resp);
    boost::asio::io_context* io;

    SignalProvider* sp;
    Signal* signalChassisState;
    Signal* signalHostState;
    Signal* signalPostDone;
    unordered_map<enum ACPILevel, Signal*> outputs;
    Dbus *dbus;
    enum ACPILevel lastLevel;
    boost::asio::deadline_timer powerCycleTimer;
    boost::asio::deadline_timer idleTimer;
    bool acpi_g3_on_host_shutdown;
};
