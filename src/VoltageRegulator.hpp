#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Logging.hpp"
#include "Signal.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/lock_guard.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

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

#define REGULATOR_EVENT_FAILURE                                                \
    (REGULATOR_EVENT_UNDER_VOLTAGE | REGULATOR_EVENT_OVER_CURRENT |            \
     REGULATOR_EVENT_REGULATION_OUT | REGULATOR_EVENT_FAIL |                   \
     REGULATOR_EVENT_OVER_TEMP | REGULATOR_EVENT_FORCE_DISABLE)

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
    // DecodeState converts the value read from
    // /sys/class/regulator/.../state
    enum RegulatorState DecodeState(string);

    // ReadState reads /sys/class/regulator/.../state
    string ReadState(void);

    // DecodeStatus converts the value read from
    // /sys/class/regulator/.../status
    enum RegulatorStatus DecodeStatus(string);

    // ReadStatus reads /sys/class/regulator/.../status
    string ReadStatus(void);

    // DecodeStatesSysfs updates the signals from sysfs attributes
    void DecodeStatesSysfs(enum RegulatorState, enum RegulatorStatus,
                           unsigned long);

    // DecodeRegulatorEvent converts the value read from
    // /sys/devices/platform/*_consumer/events
    unsigned long DecodeRegulatorEvent(string);

    // DecodeRegulatorEvent reads /sys/devices/platform/*_consumer/events
    string ReadEvents(void);

    // SetState writes to /sys/class/regulator/.../state
    void SetState(const enum RegulatorState);

    enum RegulatorState stateShadow;
    enum RegulatorStatus statusShadow;
    unsigned long eventsShadow;

    string name;
    path sysfsRoot;
    path sysfsConsumerRoot;

    bool newLevel;
    bool hasfault;

    Signal* in;
    Signal* enabled;
    Signal* fault;
    Signal* powergood;
};
