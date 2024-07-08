#pragma once

#include "Config.hpp"
#include "IODriver.hpp"
#include "Logging.hpp"
#include "Signal.hpp"

#include <boost/asio.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

#ifdef WITH_GOOGLE_TEST
#include <gtest/gtest.h>
#endif

using namespace std;

class SignalProvider;
class Signal;

#define DEFAULT_TIMEOUT_USEC 1000000

class NullVoltageRegulator :
    SignalReceiver,
    public SignalDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    // SignalReceiver's Update method for signal changes
    void Update(void);

    // Signals returns the list of signals that are feed with data
    vector<Signal*> Signals(void);

    NullVoltageRegulator(boost::asio::io_context& io,
                         boost::asio::io_service& IoOutput,
                         struct ConfigRegulator* cfg,
                         SignalProvider& prov);

  protected:
    friend class StateMachine;

  private:
    boost::asio::io_context *io;
    boost::asio::io_context *ioOutput;

    string name;

    Signal* in;
    Signal* enabled;
    Signal* fault;
    Signal* powergood;

#ifdef WITH_GOOGLE_TEST
    FRIEND_TEST(Regulator, EventParsing);
    FRIEND_TEST(Regulator, StatusParsing);
#endif
};
