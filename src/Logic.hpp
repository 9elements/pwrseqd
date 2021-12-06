#pragma once

#include "IODriver.hpp"
#include "Signal.hpp"

#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/circular_buffer.hpp>

#include <vector>

using namespace std;
class LogicInput;
class SignalProvider;
struct Config;

struct SignalChangeEvent
{
    bool Level;
    chrono::time_point<chrono::steady_clock> Time;
};

class Logic : SignalReceiver, public SignalDriver
{
  public:
    // Name returns the instance name
    string Name(void);

    // Update is called to re-evaluate the output state.
    // On level change the connected signal level is set.
    void Update(void);

    Logic(boost::asio::io_context& io, Signal* signal, string name,
          vector<LogicInput*> ands, vector<LogicInput*> ors,
          bool outputActiveLow, bool andFirst, bool invertFirst, int delay);

    Logic(boost::asio::io_service& io, SignalProvider& prov,
          struct ConfigLogic* cfg);
    ~Logic();

    // Signals returns the list of signals that are feed with data
    vector<Signal*> Signals(void);

  private:
    // GetLevelAndInputs retuns the logical 'and' of all AND inputs (ignoring
    // andThenOr)
    bool GetLevelAndInputs(void);
    // GetLevelOrInputs retuns the logical 'or' of all OR inputs (ignoring
    // andThenOr)
    bool GetLevelOrInputs(void);

    void TimerHandler(const boost::system::error_code& error,
                      const bool result);

    vector<LogicInput*> andInputs;
    vector<LogicInput*> orInputs;

    bool outputActiveLow;
    bool andThenOr;
    bool invertFirstGate;
    int delayOutputUsec;
    string name;
    time_t outputLastChanged;
    bool lastValue;
    bool lastValueValid;
    boost::asio::high_resolution_timer timer;
    Signal* signal;
    boost::circular_buffer<SignalChangeEvent> outQueue;
};
