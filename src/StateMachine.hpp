#pragma once

#include "Config.hpp"
#include "GpioInput.hpp"
#include "GpioOutput.hpp"
#include "IODriver.hpp"
#include "InventoryItemPresence.hpp"
#include "Logic.hpp"
#include "LED.hpp"
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
    StateMachine(Config&, SignalProvider&, boost::asio::io_service&, boost::asio::io_service&, Dbus&);
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

    // FOR TESTING ONLY: Sets the regulator to error state.
    void InjectRegulatorError(string name);

  protected:
    vector<NullOutput*> GetNullOutputs(void);
    vector<NullInput*> GetNullInputs(void);

  private:
    // The work guard protects the io_context from returning on idle
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard;
    boost::asio::io_context* io;
    boost::asio::io_context* ioOutput;

    // Error handler for voltage regulator failures
    void CatchVoltageRegulatorError(VoltageRegulator* vr);

    // Reference to dbus to initiate host transition on errors
    Dbus* dbus;

    vector<NullOutput*> nullOutputs;
    vector<NullInput*> nullInputs;
    vector<LED*> ledOutputs;
    vector<ItemPresent*> itemPresenceInputs;

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

  private:
    StateMachine* sm;
};
