#pragma once

#include "Config.hpp"
#include "Logging.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <filesystem>

#ifdef WITH_GOOGLE_TEST
#include <gtest/gtest.h>
#endif

using namespace std;
using namespace std::filesystem;


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
    UNDEFINED = 7,
    NOCHANGE = 8,
    INVALID = 9,
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

class VoltageRegulatorSysfs
{
  public:
    VoltageRegulatorSysfs(struct ConfigRegulator* cfg, string root = "");
    ~VoltageRegulatorSysfs();

    // DecodeState converts the value read from
    // /sys/class/regulator/.../state
    enum RegulatorState DecodeState(string);

    // DecodeState reads the value from
    // /sys/class/regulator/.../state and decodes it
    enum RegulatorState DecodeState(void);

    // ReadState reads /sys/class/regulator/.../state
    string ReadState(void);

    // ReadConsumerState reads /sys/devices/platform/*_consumer/state
    string ReadConsumerState(void);

    string StatusToString(const enum RegulatorStatus);
    string StateToString(const enum RegulatorState);
    string EventsToString(const unsigned long events);

    // DecodeStatus converts the value read from
    // /sys/class/regulator/.../status
    enum RegulatorStatus DecodeStatus(string);

    // DecodeStatus reads the value from
    // /sys/class/regulator/.../status and decodes it
    enum RegulatorStatus DecodeStatus(void);

    // ReadStatus reads /sys/class/regulator/.../status
    string ReadStatus(void);

    // DecodeRegulatorEvent converts the value read from
    // /sys/devices/platform/*_consumer/events
    unsigned long DecodeRegulatorEvent(string);

    // SetState writes to /sys/class/regulator/.../state
    void SetState(const enum RegulatorState);

    string name;

    path sysfsRoot;
    path sysfsConsumerRoot;
};
