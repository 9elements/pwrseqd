#include "LogicInput.hpp"

#include "Config.hpp"
#include "Logic.hpp"
#include "SignalProvider.hpp"

#include <chrono>
#include <iostream>

using namespace chrono;
using namespace std;

// GetLevel is called by Logic when gathering it's new state
bool LogicInput::GetLevel()
{
    if (this->inputStableUsec == 0)
    {
        this->level = this->input->GetLevel() ^ this->invert;
    }
    else
    {
        auto usec = duration_cast<microseconds>(
            steady_clock::now() - this->input->LastLevelChangeTime());

        if (usec.count() >= this->inputStableUsec)
        {
            this->level = this->input->GetLevel() ^ this->invert;
        }
        else
        {
            // Invoke the parent's Update method. It will update the signal's
            // state if necessary. It also invokes this function, which, when
            // necessary, will schedule another timer.
            this->timer.expires_from_now(boost::posix_time::microseconds(
                this->inputStableUsec - usec.count()));
            this->timer.async_wait([&](const boost::system::error_code& err) {
                if (!err)
                {
                    this->parent->Update();
                }
            });
        }
    }

    return this->level;
}

// Dependency returns the Unit attached to
Signal* LogicInput::Dependency()
{
    return this->input;
}

// Update is called whenever a signal changed
void LogicInput::Update(void)
{
    // Notify the parent if input has changed.
    this->parent->Update();
}

LogicInput::LogicInput(boost::asio::io_context& io, Signal* in, Logic* par,
                       bool inv, int stable) :
    timer(io)
{
    this->input = in;
    this->parent = par;
    this->invert = inv;
    this->inputStableUsec = stable;
    this->level = this->GetLevel();
}

LogicInput::LogicInput(boost::asio::io_context& io, SignalProvider& prov,
                       struct ConfigLogicInput* cfg, Logic* par) :
    timer(io)
{
    Signal* sig = prov.FindOrAdd(cfg->SignalName);
    sig->AddReceiver(this);

    this->input = sig;
    this->parent = par;
    this->invert = cfg->Invert;
    this->inputStableUsec = cfg->InputStableUsec;
    this->level = this->GetLevel();
}