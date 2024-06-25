#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Signal.hpp"

#include <gpiod.hpp>
#include <boost/asio/io_service.hpp>

#include <vector>

using namespace std;

class SignalProvider;
class Signal;

class GpioOutput : SignalReceiver
{
  public:
    // Name returns the instance name
    string Name(void);

    // Apply sets the new output state
    void Apply(const int newLevel);

    // SignalReceiver's Update method for signal changes
    void Update(void);

    GpioOutput(boost::asio::io_service *IoOutput,
               struct ConfigOutput* cfg,
               SignalProvider& prov);
    ~GpioOutput();

  private:
    boost::asio::io_service *ioOutput;
    int level;
    bool activeLow;
    bool DisableGpioOutCheck;
    gpiod::line line;
    gpiod::chip chip;
    Signal* in;
};