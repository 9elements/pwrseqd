

#pragma once

#include "IODriver.hpp"
#include "Signal.hpp"
#include "Dbus.hpp"

#include <boost/asio.hpp>

using namespace std;

class Signal;
class Signalprovider;
struct ConfigInput;

class ItemPresent : public SignalDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    ItemPresent(Dbus& d, struct ConfigInput* cfg, SignalProvider& prov);

    ~ItemPresent();
    void SetLevel(bool level);

    vector<Signal*> Signals(void);

  private:
    string name;
    Signal* out;
    Dbus *dbus;
};
