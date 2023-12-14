#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Signal.hpp"

#include <boost/asio.hpp>
#include <gpiod.hpp>

#include <vector>

using namespace std;

class Signal;
class Signalprovider;

class GpioInput : SignalReceiver, public SignalDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    void Poll(const boost::system::error_code& e);

    GpioInput(boost::asio::io_context& io, struct ConfigInput* cfg,
              SignalProvider& prov);

    ~GpioInput();
    // SignalDrivers's Signals method to report all driven signals
    vector<Signal*> Signals(void);

    // SignalReceiver's Update method for signal changes
    void Update(void);

  private:
    boost::asio::io_context* io;
    void OnEvent(gpiod::line_event line_event);
    void WaitForGPIOEvent(void);
    void Release(void);
    void Acquire(void);
    void TestAcquire(void);
    void SetOutputLow(void);

    boost::asio::posix::stream_descriptor streamDesc;

    bool active;
    bool ActiveLow;
    // Gate input settings
    bool GatedIdleHigh;
    bool GatedIdleLow;
    bool GatedOutputODLow;
    bool gated;

    // Signal out is set to the GPIO input value if not gated
    Signal* out;
    // Signal enable tells wether to gate the input (drive a constant level)
    // Only used when cfg->GateInput is set
    Signal* enable;

    // gpiod config
    gpiod::line line;
    gpiod::chip chip;
    ::gpiod::line_request gpiodRequestInput;
    ::std::bitset<32> gpiodFlags;
};