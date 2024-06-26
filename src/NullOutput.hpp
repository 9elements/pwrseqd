
#pragma once

#include "IODriver.hpp"
#include "Signal.hpp"

#include <boost/asio/io_service.hpp>

using namespace std;

class SignalProvider;
struct Config;

class NullOutput : SignalReceiver
{
  public:
    // Name returns the instance name
    string Name(void);

    // Apply sets the new output state
    void Apply(void);

    void Update(void);

    bool GetLevel(void);
    NullOutput(boost::asio::io_service *IoOutput,
               struct ConfigOutput* cfg,
               SignalProvider& prov);
    ~NullOutput();

  private:
    string name;
    bool active;
    bool newLevel;
    Signal* in;
};