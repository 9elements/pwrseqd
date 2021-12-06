
#include "Signal.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"
#include "StateMachine.hpp"

#include <boost/thread/lock_guard.hpp>

using namespace std;

// Name returns the instance name
string Signal::Name(void)
{
    return this->name;
}

Signal::Signal(SignalProvider* parent, string name) :
    dirty{true}, active{false}, name{name}, parent{parent},
    lastLevelChangeTime{steady_clock::now()}
{}

// GetLevel returns the internal active state
bool Signal::GetLevel()
{
    return this->active;
}

// SetLevel sets the internal active state.
// It can be called by interrupt handlers.
// It marks the signal as dirty in the parent SignalProvider.
void Signal::SetLevel(bool newLevel)
{
    if (this->active != newLevel)
    {
        boost::lock_guard<boost::mutex> guard(this->lock);
        this->active = newLevel;
        this->dirty = true;
        this->lastLevelChangeTime = steady_clock::now();
        this->parent->SetDirty(this);
        log_debug("signal " + this->Name() + " changed to " +
                  to_string(newLevel));
    }
}

steady_clock::time_point Signal::LastLevelChangeTime()
{
    return this->lastLevelChangeTime;
}

void Signal::ClearDirty(void)
{
    boost::lock_guard<boost::mutex> guard(this->lock);
    this->dirty = false;
}

bool Signal::Dirty(void)
{
    boost::lock_guard<boost::mutex> guard(this->lock);
    return this->dirty;
}

// Validate checks if the current config is sane
void Signal::Validate(vector<string>& floatingSignals)
{
    // All signals must have a receiver connected
    if (this->receivers.size() == 0)
    {
        for (auto it : floatingSignals)
        {
            if (it == this->Name())
            {
                return;
            }
        }
        throw runtime_error("no one listens to signal " + this->name);
    }
}

// Receivers returns a list of objects listening to this signal
vector<SignalReceiver*> Signal::Receivers(void)
{
    return this->receivers;
}

// AddReceiver adds a signal receiver
void Signal::AddReceiver(SignalReceiver* rec)
{
    this->receivers.push_back(rec);
}

// UpdateReceivers invokes the Update method of all signal receivers
void Signal::UpdateReceivers()
{
    for (auto dep : this->receivers)
    {
        dep->Update();
    }
}