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

class GpioOutput : SignalReceiver, public SignalDriver
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

    // SignalDrivers's Signals method to report all driven signals
    vector<Signal*> Signals(void);

  private:
    boost::asio::io_service *ioOutput;
    int level;
    bool activeLow;
    bool DisableGpioOutCheck;
    // Signal enabled is updated by the IO thread after enable was changed.
    // Depending on the IO queue length it might take several seconds to be set.
    Signal* enabled;
    gpiod::line line;
    gpiod::chip chip;
    Signal* in;
};