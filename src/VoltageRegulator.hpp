#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Logging.hpp"
#include "Signal.hpp"
#include "VoltageRegulatorSysfs.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

#ifdef WITH_GOOGLE_TEST
#include <gtest/gtest.h>
#endif

using namespace std;

class SignalProvider;
class Signal;

#define DEFAULT_TIMEOUT_USEC 1000000

class VoltageRegulator :
    SignalReceiver,
    public OutputDriver,
    public SignalDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    // Apply sets the new output state
    void Apply(void);

    // SignalReceiver's Update method for signal changes
    void Update(void);

    // Signals returns the list of signals that are feed with data
    vector<Signal*> Signals(void);

    VoltageRegulator(boost::asio::io_context& io, struct ConfigRegulator* cfg,
                     SignalProvider& prov, string root = "");
    ~VoltageRegulator();

  private:
    // Read the regulator status sysfs file
    void Poll(void);

    // ApplyStatus updates the signals from the regulator status
    // Regulator status can be set from sysfs or events
    void ApplyStatus(enum RegulatorStatus);

    // Compare the requested state against the actual state. On mismatch the error signal is set.
    void ConfirmStatusAfterTimeout(void);

    enum RegulatorState stateShadow;
    enum RegulatorStatus statusShadow;

    string name;

    // Timeout in microseconds to wait for the regulator to change it's state
    unsigned long stateChangeTimeoutUsec;
    boost::asio::deadline_timer timerStateCheck;
    boost::asio::deadline_timer timerPoll;

    // The level to be applied on next Update() call.
    bool newLevel;
    // A level change has been requested, but has not been processes yet.
    bool pendingLevelChange;
    // The level that was requested at the regulator.
    enum RegulatorState pendingNewLevel;

    Signal* in;
    Signal* enabled;
    Signal* fault;
    Signal* powergood;

    VoltageRegulatorSysfs control;

#ifdef WITH_GOOGLE_TEST
    FRIEND_TEST(Regulator, EventParsing);
    FRIEND_TEST(Regulator, StatusParsing);
#endif
};
