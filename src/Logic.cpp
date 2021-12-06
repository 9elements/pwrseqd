#include "Logic.hpp"

#include "Config.hpp"
#include "LogicInput.hpp"
#include "Signal.hpp"
#include "SignalProvider.hpp"

#include <iostream>

using namespace std;

// Name returns the instance name
string Logic::Name(void)
{
    return this->name;
}

// GetLevelOrInputs retuns the logical 'or' of all OR inputs (ignoring
// andThenOr)
bool Logic::GetLevelOrInputs(void)
{
    bool intermediate = false;

    for (auto it : this->orInputs)
    {
        if (it->GetLevel())
        {
            intermediate = true;
            break;
        }
    }

    return intermediate;
}

// GetLevelAndInputs retuns the logical 'and' of all AND inputs (ignoring
// andThenOr)
bool Logic::GetLevelAndInputs(void)
{
    bool intermediate = true;

    for (auto it : this->andInputs)
    {
        if (!it->GetLevel())
        {
            intermediate = false;
            break;
        }
    }

    return intermediate;
}

// Update is called to re-evaluate the output state.
// On level change the connected signal level is set.
void Logic::Update(void)
{
    bool result;

    if (this->andThenOr)
    {
        if (this->andInputs.size() == 0)
            result = false;
        else
        {
            result = this->GetLevelAndInputs();

            if (this->invertFirstGate)
                result = !result;
        }

        result |= this->GetLevelOrInputs();
    }
    else
    {
        if (this->orInputs.size() == 0)
            result = true;
        else
        {
            result = this->GetLevelOrInputs();

            if (this->invertFirstGate)
                result = !result;
        }

        result &= this->GetLevelAndInputs();
    }
    if (this->outputActiveLow)
        result = !result;

    if (!lastValueValid || this->lastValue != result)
    {
        if (this->delayOutputUsec > 0)
        {
            chrono::time_point<chrono::steady_clock> now =
                chrono::steady_clock().now();

            struct SignalChangeEvent event = {
                result, now + chrono::microseconds(this->delayOutputUsec)};

            this->outQueue.push_back(event);
            event = this->outQueue.begin()[0];
            if (this->outQueue.size() == 1)
            {
                this->timer.expires_after(event.Time - now);
                this->timer.async_wait([&](boost::system::error_code error) {
                    this->TimerHandler(error, event.Level);
                });
            }
        }
        else
        {
            this->signal->SetLevel(result);
        }

        this->lastValue = result;
        lastValueValid = true;
    }
}

void Logic::TimerHandler(const boost::system::error_code& error,
                         const bool result)
{
    if (!error)
    {
        struct SignalChangeEvent ev = this->outQueue.begin()[0];
        this->outQueue.pop_front();
        this->signal->SetLevel(ev.Level);
        if (this->outQueue.size() > 0)
        {
            ev = this->outQueue.begin()[0];
            this->timer.expires_after(ev.Time - chrono::steady_clock().now());
            this->timer.async_wait([&](boost::system::error_code error) {
                this->TimerHandler(error, ev.Level);
            });
        }
    }
}

vector<Signal*> Logic::Signals(void)
{
    vector<Signal*> vec;
    vec.push_back(this->signal);
    return vec;
}

Logic::Logic(boost::asio::io_context& io, Signal* signal, string name,
             vector<LogicInput*> ands, vector<LogicInput*> ors,
             bool outputActiveLow, bool andFirst, bool invertFirst, int delay) :
    timer(io),
    outQueue(100), lastValue(false), lastValueValid(false)
{
    this->signal = signal;
    this->name = name;
    this->andInputs = ands;
    this->orInputs = ors;

    this->andThenOr = andFirst;
    this->invertFirstGate = invertFirst;
    this->delayOutputUsec = delay;
}

Logic::Logic(boost::asio::io_context& io, SignalProvider& prov,
             struct ConfigLogic* cfg) :
    timer(io),
    outQueue(100), lastValue(false), lastValueValid(false)

{
    for (auto it : cfg->AndSignalInputs)
    {
        this->andInputs.push_back(new LogicInput(io, prov, &it, this));
    }

    for (auto it : cfg->OrSignalInputs)
    {
        this->orInputs.push_back(new LogicInput(io, prov, &it, this));
    }

    this->signal = prov.FindOrAdd(cfg->Out.SignalName);
    this->name = cfg->Name;
    this->andThenOr = cfg->AndThenOr;
    this->invertFirstGate = cfg->InvertFirstGate;
    this->delayOutputUsec = cfg->DelayOutputUsec;
    this->outputActiveLow = cfg->Out.ActiveLow;

    io.post([&]() { this->Update(); });
}

Logic::~Logic()
{
    for (auto it : this->andInputs)
    {
        delete it;
    }

    for (auto it : this->orInputs)
    {
        delete it;
    }
}