#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Logging.hpp"
#include "Signal.hpp"

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
using namespace std::filesystem;

class SignalProvider;
class Signal;

enum RegulatorState
{
    ENABLED = 0,
    DISABLED = 1,
    UNKNOWN = 2,
};

enum RegulatorStatus
{
    OFF = 0,
    ON = 1,
    ERROR = 2,
    FAST = 3,
    NORMAL = 4,
    IDLE = 5,
    STANDBY = 6,
    NOCHANGE = 7,
};

#define REGULATOR_EVENT_UNDER_VOLTAGE 0x01
#define REGULATOR_EVENT_OVER_CURRENT 0x02
#define REGULATOR_EVENT_REGULATION_OUT 0x04
#define REGULATOR_EVENT_FAIL 0x08
#define REGULATOR_EVENT_OVER_TEMP 0x10
#define REGULATOR_EVENT_FORCE_DISABLE 0x20
#define REGULATOR_EVENT_VOLTAGE_CHANGE 0x40
#define REGULATOR_EVENT_DISABLE 0x80
#define REGULATOR_EVENT_PRE_VOLTAGE_CHANGE 0x100
#define REGULATOR_EVENT_ABORT_VOLTAGE_CHANGE 0x200
#define REGULATOR_EVENT_PRE_DISABLE 0x400
#define REGULATOR_EVENT_ABORT_DISABLE 0x800
#define REGULATOR_EVENT_ENABLE 0x1000
/*
 * Following notifications should be emitted only if detected condition
 * is such that the HW is likely to still be working but consumers should
 * take a recovery action to prevent problems esacalating into errors.
 */
#define REGULATOR_EVENT_UNDER_VOLTAGE_WARN 0x2000
#define REGULATOR_EVENT_OVER_CURRENT_WARN 0x4000
#define REGULATOR_EVENT_OVER_VOLTAGE_WARN 0x8000
#define REGULATOR_EVENT_OVER_TEMP_WARN 0x10000
#define REGULATOR_EVENT_WARN_MASK 0x1E000

#define REGULATOR_EVENT_EN_DIS                                                 \
    (REGULATOR_EVENT_ENABLE | REGULATOR_EVENT_DISABLE)
#define REGULATOR_EVENT_FAILURE                                                \
    (REGULATOR_EVENT_UNDER_VOLTAGE | REGULATOR_EVENT_OVER_CURRENT |            \
     REGULATOR_EVENT_REGULATION_OUT | REGULATOR_EVENT_FAIL |                   \
     REGULATOR_EVENT_OVER_TEMP | REGULATOR_EVENT_FORCE_DISABLE)

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

    // DecodeEvents turns an bitmask to status
    enum RegulatorStatus DecodeEvents(unsigned long events);

    // Signals returns the list of signals that are feed with data
    vector<Signal*> Signals(void);

    VoltageRegulator(boost::asio::io_context& io, struct ConfigRegulator* cfg,
                     SignalProvider& prov, string root = "");
    ~VoltageRegulator();

  private:
    // DecodeState converts the value read from
    // /sys/class/regulator/.../state
    enum RegulatorState DecodeState(string);

    // ReadState reads /sys/class/regulator/.../state
    string ReadState(void);

    // ReadConsumerState reads /sys/devices/platform/*_consumer/state
    string ReadConsumerState(void);

    string StatusToString(const enum RegulatorStatus);
    string StateToString(const enum RegulatorState);

    // DecodeStatus converts the value read from
    // /sys/class/regulator/.../status
    enum RegulatorStatus DecodeStatus(string);
#ifdef WITH_GOOGLE_TEST
    FRIEND_TEST(Regulator, StatusParsing);
#endif

    // ReadStatus reads /sys/class/regulator/.../status
    string ReadStatus(void);

    // ApplyStatus updates the signals from the regulator status
    // Regulator status can be set from sysfs or events
    void ApplyStatus(enum RegulatorStatus);

    // DecodeRegulatorEvent converts the value read from
    // /sys/devices/platform/*_consumer/events
    unsigned long DecodeRegulatorEvent(string);
#ifdef WITH_GOOGLE_TEST
    FRIEND_TEST(Regulator, EventParsing);
#endif
    // DecodeRegulatorEvent reads /sys/devices/platform/*_consumer/events
    string ReadEvents(void);

    // SetState writes to /sys/class/regulator/.../state
    void SetState(const enum RegulatorState);

    // Compare the requested state against the actual state. On mismatch the error signal is set.
    void ConfirmStatusAfterTimeout(void);

    enum RegulatorState stateShadow;
    enum RegulatorStatus statusShadow;

    string name;
    path sysfsRoot;
    path sysfsConsumerRoot;

    // Timeout in microseconds to wait for the regulator to change it's state
    unsigned long stateChangeTimeoutUsec;
    boost::asio::deadline_timer timer;

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
};
