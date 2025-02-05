#pragma once

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <vector>

using namespace std;

struct ConfigLogicOutput
{
    // The signal that is driven by this block
    string SignalName;
    // ActiveLow inverts the logic level of the signal when set
    bool ActiveLow;
};

struct ConfigLogicInput
{
    // The signal where the logic level is read from
    string SignalName;
    // Invert invertes the input signal level
    bool Invert;
    // InputStableUsec specifies the time where the input level needs to
    // be constant
    int InputStableUsec;
};

struct ConfigLogic
{
    // Name of the logic Unit
    string Name;
    // List of input signals that are logical ANDed
    vector<ConfigLogicInput> AndSignalInputs;
    // List of input signals that are logical ORed
    vector<ConfigLogicInput> OrSignalInputs;
    // AndThenOr gives the relation between AND and OR gates
    //      True:             False
    // - |                  -------- |
    // - | AND - |          -------- | AND -
    // - |       |          -------- |
    //           | OR -              |
    // ----------|          - | OR - |
    // ----------|          - |
    bool AndThenOr;
    // InvertFirstGate turns the first logical gate into
    // and NAND or an NOR gatter (depending which is the first).
    bool InvertFirstGate;
    // DelayOutputUsec adds a fixed delay to every level change
    int DelayOutputUsec;
    // Out configures which Signal is driven by this logic block
    ConfigLogicOutput Out;
};

enum ConfigInputType
{
    INPUT_TYPE_GPIO,
    INPUT_TYPE_DBUS_PRESENCE,
    INPUT_TYPE_NULL,
    INPUT_TYPE_UNKNOWN,
};

enum ConfigOutputType
{
    OUTPUT_TYPE_GPIO,
    OUTPUT_TYPE_LED,
    OUTPUT_TYPE_NULL,
    OUTPUT_TYPE_LOGGING,
    OUTPUT_TYPE_UNKNOWN,
};

struct ConfigInput
{
    // The name where the signal level is read from
    // For DBUS it's the inventory path.
    // For GPIO it's the GPIO line name.
    string Name;
    // The chipname where the signal level is read from (only for GPIOs)
    string GpioChipName;
    // The signal where the logic level is applied to
    string SignalName;
    // ActiveLow invertes the input signal level
    bool ActiveLow;
    // PullUp configures GPIO to use internal pull up bias
    bool PullUp;
    // PullDown configures GPIO to use internal pull down bias
    bool PullDown;
    // BiasDisable configures GPIO to use no internal bias
    bool DisableBias;
    // GateInput adds an "On" input signal to the input driver
    // When low the input gpio isn't read any more and the GatedIdleHigh
    // or GatedIdleLow is used instead
    bool GateInput;
    // When set and GateInput is set and On input signal
    // of the driver is low then a high signal is driven.
    // The ActiveLow setting will invert this signal if set.
    // You cannot select GatedIdleHigh and GatedIdleLow at the same time
    bool GatedIdleHigh;
    // When set and GateInput is set and On input signal
    // of the driver is low then a low signal is driven.
    // The ActiveLow setting will invert this signal if set.
    // You cannot select GatedIdleHigh and GatedIdleLow at the same time.
    bool GatedIdleLow;
    // When set and GateInput is set and On input signal of the driver
    // is low then the pin is set to output and driven low (OpenDrain Low).
    // When On input signal of the driver is high then the pin is input.
    bool GatedOutputODLow;

    // Description is just for debugging purposes
    string Description;
    // Type specifies the input backend to use
    enum ConfigInputType InputType;
};

struct ConfigOutput
{
    // The name where the signal level is appied to
    string Name;
    // The chipname where the signal level is read from (only for GPIOs)
    string GpioChipName;
    // The signal where the logic level is read from
    string SignalName;
    // ActiveLow invertes the input signal level
    bool ActiveLow;
    // OpenDrain configures GPIO as open drain pin
    bool OpenDrain;
    // OpenSource configures GPIO as open source pin
    bool OpenSource;
    // PullUp configures GPIO to use internal pull up bias
    bool PullUp;
    // PullDown configures GPIO to use internal pull down bias
    bool PullDown;
    // BiasDisable configures GPIO to use no internal bias
    bool DisableBias;
    // Description is just for debugging purposes
    string Description;
    // Type specifies the output backend to use
    enum ConfigOutputType OutputType;

    // When set indicate to ignore doing gpio out check.
    // This is perticularly needed when gpio is 3.3V open drain
    // but signal is pullup to 1V which always read back as 0V or logic low.
    bool DisableGpioOutCheck;
};

struct ConfigRegulator
{
    // The regulator name
    string Name;
    // Description is just for debugging purposes
    string Description;
    // Optional Timeout in microseconds to wait for regulator state to change
    unsigned long TimeoutUsec;
    // Use dummy regulator instead of real regulator
    bool IsDummy;
    // Allow regulator to be missing. A dummy regulator will be spawned instead.
    bool AllowMissing;
    // Allow regulator to fault. A fault will not shut down the system to ACPI G3.
    bool AllowFaulting;
};

struct ConfigImmutable
{
    // The signal where the logic level is applied to
    string SignalName;
    // Level is the level the signal always drives
    bool Level;
};

struct ConfigACPIStates
{
    // Name is the name of the ACPI state
    string Name;
    // Initial is true for the ACPI state at power on
    bool Initial;
};

struct Config
{
    vector<ConfigLogic> Logic;
    vector<ConfigInput> Inputs;
    vector<ConfigOutput> Outputs;
    vector<string> FloatingSignals;
    vector<ConfigRegulator> Regulators;
    vector<ConfigImmutable> Immutables;
    vector<ConfigACPIStates> ACPIStates;
};

Config LoadConfig(string path);
