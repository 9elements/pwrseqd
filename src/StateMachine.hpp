#pragma once

#include "Config.hpp"
#include "GpioInput.hpp"
#include "GpioOutput.hpp"
#include "IODriver.hpp"
#include "Logic.hpp"
#include "NullInput.hpp"
#include "NullOutput.hpp"
#include "Signal.hpp"
#include "SignalProvider.hpp"
#include "Validate.hpp"
#include "VoltageRegulator.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/thread/mutex.hpp>

#include <vector>

using namespace std;

class StateMachineTester;

class StateMachine : Validator
{
  public:
    // Create statemachine from config
    StateMachine(Config&, SignalProvider&, boost::asio::io_service& io);
    ~StateMachine();

    // Run starts the internal state machine.
    //
    // The method is has work to do when at least one signal had been scheduled
    // by a call to ScheduleSignalChange.
    //
    // It calls

    void Run(void);
    void Poll(void);

    void EvaluateState(void);

    // OnDirtySet is called when a signal has the dirty bit set
    void OnDirtySet(void);

    // Validates checks if the current config is sane
    void Validate(void);

  protected:
    vector<NullOutput*> GetNullOutputs(void);
    vector<NullInput*> GetNullInputs(void);

    // ApplyOutputSignalLevel applies the new signal state.
    // This will change the output of GPIO pins or enable/disable voltage
    // regulators.
    void ApplyOutputSignalLevel(void);

  private:
    // The work guard protects the io_context from returning on idle
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard;
    boost::asio::io_context* io;

    vector<OutputDriver*> outputDrivers;
    vector<NullOutput*> nullOutputs;
    vector<NullInput*> nullInputs;

    vector<VoltageRegulator*> voltageRegulators;

    vector<GpioOutput*> gpioOutputs;
    vector<GpioInput*> gpioInputs;
    vector<Logic*> logic;

    SignalProvider* sp;
    friend class StateMachineTester;
};

class StateMachineTester
{
  public:
    vector<NullOutput*> GetNullOutputs(void)
    {
        return sm->GetNullOutputs();
    }
    vector<NullInput*> GetNullInputs(void)
    {
        return sm->GetNullInputs();
    }
    StateMachineTester(StateMachine* sm) : sm{sm} {};

    void ApplyOutputSignalLevel(void)
    {
        sm->ApplyOutputSignalLevel();
    }

  private:
    StateMachine* sm;
};
