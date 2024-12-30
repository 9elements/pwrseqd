#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Signal.hpp"

#include <boost/asio/io_service.hpp>

#include <vector>

using namespace std;

class SignalProvider;
class Signal;

class LogOutput : SignalReceiver
{
  public:
    // Name returns the instance name
    string Name(void);

    // Apply sets the new output state
    void Apply(const int newLevel);

    // SignalReceiver's Update method for signal changes
    void Update(void);

    LogOutput(boost::asio::io_service& Io,
              boost::asio::io_service *IoOutput,
              struct ConfigOutput* cfg,
              SignalProvider& prov);

  private:
    std::string name;
    boost::asio::io_service *io;
    boost::asio::io_service *ioOutput;
    int level;
    bool activeLow;
    Signal* in;
    Signal* enable;
    std::string errorString;
};