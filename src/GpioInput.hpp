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

class GpioInput : public SignalDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    void Poll(const boost::system::error_code& e);

    GpioInput(boost::asio::io_context& io, struct ConfigInput* cfg,
              SignalProvider& prov);

    ~GpioInput();
    vector<Signal*> Signals(void);

  private:
    void OnEvent(gpiod::line_event line_event);
    void WaitForGPIOEvent(void);

    boost::asio::posix::stream_descriptor streamDesc;

    bool active;
    gpiod::line line;
    gpiod::chip chip;
    Signal* out;
};