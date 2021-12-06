#pragma once

#include "Validate.hpp"

#include <boost/thread/mutex.hpp>

#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

class StateMachine;
class SignalProvider;
class GpioOutput;
class NullOutput;
class LogicInput;
class VoltageRegulator;
class ACPIStates;

// The signal receiver reads the current level from the Signal.
// The signal keeps track of all receivers.
class SignalReceiver
{
  public:
    virtual void Update(void) = 0;
};

class Signal
{
  public:
    // Name returns the instance name
    string Name(void);

    Signal(SignalProvider* parent, string name);

    // GetLevel returns the internal active state
    bool GetLevel();

    // SetLevel sets the internal active state.
    // It can be called by interrupt handlers.
    // It marks the signal as dirty in the parent SignalProvider.
    void SetLevel(bool);

    // Validate checks if the current config is sane
    void Validate(vector<string>&);

    // Dirty is set when SetLevel changed the level
    bool Dirty(void);

    // ClearDirty clears the dirty bit
    void ClearDirty(void);

    // LastLevelChangeTime returns the time when the signal level was changed
    steady_clock::time_point LastLevelChangeTime();

    // Receivers returns a pointer to a vector of objects listening to this
    // signal
    vector<SignalReceiver*> Receivers(void);

    // UpdateReceivers invokes the Update method of all signal receivers
    void UpdateReceivers();

  protected:
    // AddReceiver adds a signal receiver
    void AddReceiver(SignalReceiver* rec);
    friend GpioOutput;
    friend NullOutput;
    friend LogicInput;
    friend VoltageRegulator;
    friend ACPIStates;

  private:
    SignalProvider* parent;
    // Internal state of the signal. Can only be modified by a call to Apply().
    bool active;
    bool dirty;
    string name;

    boost::mutex lock;
    steady_clock::time_point lastLevelChangeTime;

    vector<SignalReceiver*> receivers;
};
