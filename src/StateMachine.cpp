
#include "StateMachine.hpp"

#include "Logging.hpp"

#include <boost/thread/lock_guard.hpp>

#include <functional>
using namespace std;
using namespace placeholders;

// Create statemachine from config
StateMachine::StateMachine(Config& cfg, SignalProvider& prov,
                           boost::asio::io_service& io,
                           boost::asio::io_service& IoOutput,
                           Dbus& dbus) :
    io{&io}, ioOutput{&IoOutput},
    work_guard{io.get_executor()},
    dbus(&dbus)
{

    prov.SetDirtyBitEvent([this](void) { this->OnDirtySet(); });

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
            GpioInput* g = new GpioInput(io, IoOutput, &cfg.Inputs[i], prov);
            this->gpioInputs.push_back(g);
            prov.AddDriver(g);
            log_debug("pushing gpio input " + cfg.Inputs[i].SignalName);
        }
        else if (cfg.Inputs[i].InputType == INPUT_TYPE_DBUS_PRESENCE)
        {
            ItemPresent* p = new ItemPresent(dbus, &cfg.Inputs[i], prov);
            this->itemPresenceInputs.push_back(p);
            prov.AddDriver(p);
            log_debug("using DBus presence input " + cfg.Inputs[i].SignalName);
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
            GpioOutput* g = new GpioOutput(io, &IoOutput, &cfg.Outputs[i], prov);
            this->gpioOutputs.push_back(g);
            prov.AddDriver(g);
            log_debug("using gpio output " + cfg.Outputs[i].SignalName);
        }
        else if (cfg.Outputs[i].OutputType == OUTPUT_TYPE_LED)
        {
            LED* l = new LED(&IoOutput, dbus, &cfg.Outputs[i], prov);
            this->ledOutputs.push_back(l);
            log_debug("using LED output " + cfg.Outputs[i].SignalName);
        }
        else if (cfg.Outputs[i].OutputType == OUTPUT_TYPE_NULL)
        {
            NullOutput* n = new NullOutput(&IoOutput, &cfg.Outputs[i], prov);
            this->nullOutputs.push_back(n);
            log_debug("using null output " + cfg.Outputs[i].SignalName);
        }
        else if (cfg.Outputs[i].OutputType == OUTPUT_TYPE_LOGGING)
        {
            LogOutput* l = new LogOutput(io, &IoOutput, &cfg.Outputs[i], prov);
            this->logOutputs.push_back(l);
            log_debug("using log output " + cfg.Outputs[i].SignalName);
        }
    }

    for (int i = 0; i < cfg.Regulators.size(); i++)
    {
        if (cfg.Regulators[i].IsDummy)
        {
            NullVoltageRegulator* v =
                new NullVoltageRegulator(io, IoOutput, &cfg.Regulators[i], prov);
            prov.AddDriver(v);
            log_debug("using dummy voltage regulator " + cfg.Regulators[i].Name);
        } else{
            try {
                VoltageRegulator* v =
                    new VoltageRegulator(io, IoOutput, &cfg.Regulators[i], prov, "",
                        [this](VoltageRegulator* vr) { this->CatchVoltageRegulatorError(vr); });
                this->voltageRegulators.push_back(v);
                prov.AddDriver(v);
            } catch (const std::exception& ex) {
                if (cfg.Regulators[i].AllowMissing) {
                    log_err("Failed to use regulator " + cfg.Regulators[i].Name + " : " + ex.what());
                    log_sel("Failed to use regulator " + cfg.Regulators[i].Name + " : " + ex.what() + ". Using dummy regulator.",
                            "/xyz/openbmc_project/inventory/system/chassis/motherboard", false);
                    NullVoltageRegulator* v =
                        new NullVoltageRegulator(io, IoOutput, &cfg.Regulators[i], prov);
                    prov.AddDriver(v);
                    log_debug("using dummy voltage regulator " + cfg.Regulators[i].Name);
                } else {
                    log_sel("Failed to use regulator " + cfg.Regulators[i].Name + " : " + ex.what(),
                            "/xyz/openbmc_project/inventory/system/chassis/motherboard", false);
                    throw;
                }
            }
        }
    }

    this->sp = &prov;
}

StateMachine::~StateMachine(void)
{
    this->work_guard.reset();
}

void StateMachine::CatchVoltageRegulatorError(VoltageRegulator* vr)
{
    log_err("Switching off host due to failure of regulator " + vr->Name());

    // Turn off everything
    this->dbus->RequestedPowerTransition(dbus::ChassisTransition::off);
}

void StateMachine::InjectRegulatorError(string name)
{
    for (auto it : this->voltageRegulators) {
      if (it->Name() == name) {
          log_debug(name + ": Injecting error event!");
          it->ApplyStatus(ERROR);
      }
    }
}

// Validate checks if the current config is sane
void StateMachine::Validate(void)
{
    this->sp->Validate();
}

// OnDirtySet is invoked when a signal dirty bit is set
void StateMachine::OnDirtySet(void)
{
    this->io->post([this]() { this->EvaluateState(); });
}

// EvaluateState runs until no more signals change
void StateMachine::EvaluateState(void)
{
    bool dirty = false;
    int timeout = 10000;

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
    if (timeout == 0)
    {
        log_err("Failed to evaluate stable state, trying again...");
        this->OnDirtySet();
    }
}

// Run does work on the io_queue.
void StateMachine::Run(void)
{
    this->io->post([this]() { this->EvaluateState(); });
    this->io->post([this]() { this->sp->PrintSignals(); });

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