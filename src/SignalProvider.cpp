
#include "SignalProvider.hpp"

#include "Config.hpp"
#include "IODriver.hpp"
#include "Logging.hpp"
#include "Signal.hpp"

#include <chrono>
#include <filesystem>

using namespace std;
using namespace std::chrono;
using namespace std::filesystem;

SignalProvider::SignalProvider(Config& cfg, string dumpFolder) :
    floatingSignals{cfg.FloatingSignals}, dumpFolder{dumpFolder},
    dirtyBitSignal{nullptr}, dirtyAIsActive{false}
{
    for (auto it : cfg.Immutables)
    {
        Signal* s = this->FindOrAdd(it.SignalName);
        s->SetLevel(it.Level);
        immutables.push_back(s);
    }
    if (dumpFolder != "")
    {
        path f(dumpFolder / path("signals.txt"));
        this->outfile.open(f.string(), ofstream::out | ofstream::app);
    }
}

SignalProvider::~SignalProvider()
{

    if (this->outfile.is_open())
    {
        this->outfile.close();

        string cmd =
            "gnuplot -e \"unset ytics; set grid x; set xtics rotate 1000; set yrange [-0.5:" +
            to_string(this->signals.size() * 2 + 1) +
            "]; set xlabel 'microseconds'; set term svg size 4096 " +
            to_string(256 * this->signals.size()) +
            " dynamic ; set output 'output.svg'; set key outside; ";
        cmd += "plot for [col=2:" + to_string(this->signals.size() + 1) +
               "] '" + this->dumpFolder +
               "/signals.txt' using 1:col with lines title columnheader;";

        cout << cmd << endl;
        cmd += "\"";
        system(cmd.c_str());
    }
    for (auto it : this->signals)
    {
        delete it.second;
    }

    this->signals.clear();
    this->dirtyA.clear();
    this->dirtyB.clear();
}

// Add a new signal and take ownership of it.
// Should not be called at runtime.
Signal* SignalProvider::Add(string name)
{
    Signal* s;
    if (name == "")
        throw runtime_error("Adding signal without name");

    s = new Signal(this, name);

    // All signals start dirty. Track them now for first statemachine
    // invokation.
    if (this->dirtyAIsActive)
    {
        this->dirtyA.push_back(s);
    }
    else
    {
        this->dirtyB.push_back(s);
    }
    // Give the dirty vector a hint how much signals might end in it.
    this->dirtyA.reserve(this->signals.size());
    this->dirtyB.reserve(this->signals.size());

    this->signals[name] = s;
    return s;
}

// Find returns a signal by name. NULL if not found.
Signal* SignalProvider::Find(string name)
{
    auto search = signals.find(name);
    if (search == signals.end())
        return nullptr;

    return this->signals[name];
}

// FindOrAdd returns a signal by name. If not found a new signal is added
Signal* SignalProvider::FindOrAdd(string name)
{
    Signal* s;
    s = this->Find(name);
    if (s != nullptr)
        return s;
    return this->Add(name);
}

// GetDirtySignalsAndClearList provides a pointer to vector of signals
// that had the "dirty" bit set. The invokation unsets the dirty bit,
// clears the internal list and returns a copy to it that can be used until this
// function is called again.
vector<Signal*>* SignalProvider::GetDirtySignalsAndClearList()
{
    this->ClearDirty();
    if (this->dirtyAIsActive)
    {
        this->dirtyAIsActive = false;
        this->dirtyB.clear();

        return &this->dirtyA;
    }
    else
    {
        this->dirtyAIsActive = true;
        this->dirtyA.clear();

        return &this->dirtyB;
    }
}

// ClearDirty removes the dirty bit of all signals and clears the list
void SignalProvider::ClearDirty(void)
{
    if (this->dirtyAIsActive)
    {
        for (auto it : this->dirtyA)
        {
            it->ClearDirty();
        }
    }
    else
    {
        for (auto it : this->dirtyB)
        {
            it->ClearDirty();
        }
    }
}

// SetDirty adds the signal to the dirty listt
void SignalProvider::SetDirty(Signal* sig)
{
    vector<Signal*>* vec;
    if (this->dirtyAIsActive)
        vec = &this->dirtyA;
    else
        vec = &this->dirtyB;

    // FIXME: Remove once signal dirty state is thread safe
    for (auto it : *vec)
    {
        if (it == sig)
        {
            return;
        }
    }
    vec->push_back(sig);

    // Invoke dirty bit listeners
    this->dirtyBitSignal();
}

// SetDirtyBitEvent adds an event handler for dirty bit set events
void SignalProvider::SetDirtyBitEvent(std::function<void(void)> const& lamda)
{
    this->dirtyBitSignal = lamda;
}

void SignalProvider::AddDriver(SignalDriver* drv)
{
    this->signalDrivers.push_back(drv);
}

void SignalProvider::Validate()
{
    // Check if signal drives something
    for (auto it : this->signals)
    {
        it.second->Validate(this->floatingSignals);
    }

    // For each signal try to find a input driver that sources the signal
    for (auto it : this->signals)
    {
        bool found = false;
        for (auto d : this->signalDrivers)
        {
            for (auto s : d->Signals())
            {
                if (s == it.second)
                {
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        for (auto i : this->immutables)
        {
            if (i == it.second)
            {
                found = true;
                break;
            }
        }
        if (!found)
            throw runtime_error("no one drives signal " + it.second->Name());
    }
}

void SignalProvider::PrintSignals(void)
{
    // Check if signal drives something
    for (auto it : this->signals)
    {
        log_debug("signal " + it.first + " = " +
                  to_string(it.second->GetLevel()));
    }
}

void SignalProvider::DumpSignals(void)
{
    static bool once;
    static long long start;
    if (!once)
    {
        start = duration_cast<microseconds>(
                    high_resolution_clock::now().time_since_epoch())
                    .count();
        once = true;

        outfile << "time"
                << " ";
        // Print columns headers
        for (auto it : this->signals)
        {
            outfile << it.first << " ";
        }
        outfile << endl;
    }

    this->outfile << duration_cast<microseconds>(
                         high_resolution_clock::now().time_since_epoch())
                             .count() -
                         start;
    this->outfile << " ";

    int i = 0;
    for (auto it : this->signals)
    {
        outfile << (it.second->GetLevel() ? (i + 1) : i) << " ";
        i += 2;
    }
    this->outfile << endl;
}
