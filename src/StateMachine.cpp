
#include "StateMachine.hpp"

#include "Logging.hpp"

#include <boost/thread/lock_guard.hpp>

#include <functional>
using namespace std;
using namespace placeholders;

// Create statemachine from config
StateMachine::StateMachine(Config& cfg, SignalProvider& prov,
                           boost::asio::io_service& io) :
    io{&io},
    work_guard{io.get_executor()}
{

    prov.SetDirtyBitEvent([&](void) { this->OnDirtySet(); });

    for (int i = 0; i < cfg.Logic.size(); i++)
    {
        Logic* l = new Logic(io, prov, &cfg.Logic[i]);
        this->logic.push_back(l);
        prov.AddDriver(l);
        log_debug("using logic " + cfg.Logic[i].Name);
    }
    for (int i = 0; i < cfg.Inputs.size(); i++)
    {
        if (cfg.Inputs[i].InputType == INPUT_TYPE_GPIO)
        {
            GpioInput* g = new GpioInput(io, &cfg.Inputs[i], prov);
            this->gpioInputs.push_back(g);
            prov.AddDriver(g);
            log_debug("pushing gpio input " + cfg.Inputs[i].SignalName);
        }
        else if (cfg.Inputs[i].InputType == INPUT_TYPE_NULL)
        {
            NullInput* n = new NullInput(io, &cfg.Inputs[i], prov);
            this->nullInputs.push_back(n);
            prov.AddDriver(n);
            log_debug("using null input " + cfg.Inputs[i].SignalName);
        }
    }

    for (int i = 0; i < cfg.Outputs.size(); i++)
    {
        if (cfg.Outputs[i].OutputType == OUTPUT_TYPE_GPIO)
        {
            GpioOutput* g = new GpioOutput(&cfg.Outputs[i], prov);
            this->gpioOutputs.push_back(g);
            this->outputDrivers.push_back(g);
            log_debug("using gpio output " + cfg.Outputs[i].SignalName);
        }
        else if (cfg.Outputs[i].OutputType == OUTPUT_TYPE_NULL)
        {
            NullOutput* n = new NullOutput(&cfg.Outputs[i], prov);
            this->nullOutputs.push_back(n);
            this->outputDrivers.push_back(n);
            log_debug("using null output " + cfg.Outputs[i].SignalName);
        }
    }

    for (int i = 0; i < cfg.Regulators.size(); i++)
    {
        VoltageRegulator* v =
            new VoltageRegulator(io, &cfg.Regulators[i], prov);
        this->voltageRegulators.push_back(v);
        this->outputDrivers.push_back(v);
        prov.AddDriver(v);
        log_debug("using voltage regulator " + cfg.Regulators[i].Name);
    }

    this->sp = &prov;
}

StateMachine::~StateMachine(void)
{
    this->work_guard.reset();
}

// Validate checks if the current config is sane
void StateMachine::Validate(void)
{
    this->sp->Validate();
}

// ApplyOutputSignalLevel writes signal state to outputs
void StateMachine::ApplyOutputSignalLevel(void)
{
    for (auto it : this->outputDrivers)
    {
        it->Apply();
    }
}

// OnDirtySet is invoked when a signal dirty bit is set
void StateMachine::OnDirtySet(void)
{
    this->io->post([&]() { this->EvaluateState(); });
}

// EvaluateState runs until no more signals change
void StateMachine::EvaluateState(void)
{
    bool dirty = false;
    int timeout = 10000;

    log_debug("EvaluateState entering");
    if (_loglevel > 2)
        this->sp->PrintSignals();

    vector<Signal*>* signals = this->sp->GetDirtySignalsAndClearList();
    dirty = signals->size() > 0;

    while (signals->size() > 0 && timeout > 0)
    {
        if (_loglevel > 2)
            log_debug("Dirty signals:");

        /* Invoke Update() method of signal listeners */
        for (auto sig : *signals)
        {
            if (_loglevel > 2)
                log_debug(sig->Name() + " = " + to_string(sig->GetLevel()));

            sig->UpdateReceivers();
        }
        // The Update call might have added new dirty signals.
        // FIXME: Add timeout and loop detection.
        signals = this->sp->GetDirtySignalsAndClearList();
        timeout--;
    }
    log_debug("EvaluateState done");
    if (timeout == 0)
    {
        log_err("Failed to evaluate stable state, trying again...");
        this->OnDirtySet();
    }

    if (_loglevel > 2)
        this->sp->PrintSignals();

    // State is stable
    if (dirty)
        this->ApplyOutputSignalLevel();
}

// Run does work on the io_queue.
void StateMachine::Run(void)
{
    this->io->post([&]() { this->EvaluateState(); });

    this->io->run();
}

void StateMachine::Poll(void)
{
    this->io->poll();
}

vector<NullOutput*> StateMachine::GetNullOutputs(void)
{
    return this->nullOutputs;
}

vector<NullInput*> StateMachine::GetNullInputs(void)
{
    return this->nullInputs;
}