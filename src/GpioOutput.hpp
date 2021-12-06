#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Signal.hpp"

#include <gpiod.hpp>

#include <vector>

using namespace std;

class SignalProvider;
class Signal;

class GpioOutput : SignalReceiver, public OutputDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    // Apply sets the new output state
    void Apply(void);

    // SignalReceiver's Update method for signal changes
    void Update(void);

    GpioOutput(struct ConfigOutput* cfg, SignalProvider& prov);
    ~GpioOutput();

  private:
    bool active;
    bool newLevel;
    bool activeLow;
    gpiod::line line;
    gpiod::chip chip;
    Signal* in;
};