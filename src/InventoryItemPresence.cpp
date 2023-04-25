#include "InventoryItemPresence.hpp"

#include "Config.hpp"
#include "Logging.hpp"
#include "Signal.hpp"
#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string ItemPresent::Name(void)
{
    return this->name;
}

ItemPresent::ItemPresent(Dbus& d, struct ConfigInput* cfg, SignalProvider& prov) :
    name(cfg->Name), dbus(&d)
{
    this->out = prov.FindOrAdd(cfg->SignalName);
    this->name = cfg->Name;
    this->dbus->RegisterItemPresentCallback(this->name,
        [this](const bool present) {
            log_debug("DBus Item presence '" + this->Name() + "' changed to " +
                (present ? "present" : "not present"));
            this->out->SetLevel(present);
    });
    this->dbus->GetItemPresentCallback(this->name,
        [this](const bool present) {
            log_debug("DBus Item presence '" + this->Name() + "' is " +
                (present ? "present" : "not present"));
            this->out->SetLevel(present);
    });
}

vector<Signal*> ItemPresent::Signals(void)
{
    vector<Signal*> vec;
    vec.push_back(this->out);
    return vec;
}

ItemPresent::~ItemPresent()
{}
