
#include "LED.hpp"

#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string LED::Name(void)
{
    return "LED_" + this->name;
}

void LED::Apply(void)
{
    if (this->newLevel != this->level)
    {
        this->level = this->newLevel;
        this->dbus->SetLEDState(this->name, this->level);
    }
}

void LED::Update(void)
{
    this->newLevel = this->in->GetLevel() ^ this->activeLow;
    ioOutput->post([this]() {
        this->Apply();
    });
}

LED::LED(boost::asio::io_service *IoOutput, Dbus& d, struct ConfigOutput* cfg, SignalProvider& prov) :
    ioOutput(IoOutput), level(false), newLevel(false), activeLow(cfg->ActiveLow), name(cfg->Name), dbus(&d)
{
    this->in = prov.FindOrAdd(cfg->SignalName);
    this->in->AddReceiver(this);

    this->Update();
    this->level = this->newLevel;
    this->dbus->SetLEDState(this->name, this->level);
}