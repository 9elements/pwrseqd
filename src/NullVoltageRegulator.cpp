
#include "NullVoltageRegulator.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string NullVoltageRegulator::Name(void)
{
    return this->name;
}

vector<Signal*> NullVoltageRegulator::Signals(void)
{
    vector<Signal*> vec;

    vec.push_back(this->enabled);
    vec.push_back(this->powergood);
    vec.push_back(this->fault);
    vec.push_back(this->dummy);

    return vec;
}

void NullVoltageRegulator::Update(void)
{
}

NullVoltageRegulator::NullVoltageRegulator(boost::asio::io_context& io,
                                   boost::asio::io_service& IoOutput,
                                   struct ConfigRegulator* cfg,
                                   SignalProvider& prov) :
    io(&io), ioOutput(&IoOutput), name(cfg->Name)
{
    this->in = prov.FindOrAdd(cfg->Name + "_On");
    this->in->AddReceiver(this);

    this->enabled = prov.FindOrAdd(cfg->Name + "_Enabled");
    this->fault = prov.FindOrAdd(cfg->Name + "_Fault");
    this->powergood = prov.FindOrAdd(cfg->Name + "_PowerGood");
    this->dummy = prov.FindOrAdd(cfg->Name + "_IsDummy");

    // Set initial signal levels
    this->fault->SetLevel(false);
    this->powergood->SetLevel(false);
    this->enabled->SetLevel(false);
    this->dummy->SetLevel(true);
}
