#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Signal.hpp"
#include "Dbus.hpp"

#include <boost/asio.hpp>

using namespace std;

class SignalProvider;
class Signal;

class LED : SignalReceiver, public OutputDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    // Apply sets the new output state
    void Apply(void);

    // SignalReceiver's Update method for signal changes
    void Update(void);

    LED(Dbus& d, struct ConfigOutput* cfg, SignalProvider& prov);

  private:
    bool level;
    bool newLevel;
    bool activeLow;
    string name;
    Signal* in;
    Dbus *dbus;
};