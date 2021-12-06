
#include "NullOutput.hpp"

#include "Config.hpp"
#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string NullOutput::Name(void)
{
    return this->name;
}

void NullOutput::Apply(void)
{
    if (this->newLevel != this->active)
    {
        this->active = this->newLevel;
    }
}

void NullOutput::Update(void)
{
    this->newLevel = this->in->GetLevel();
}

bool NullOutput::GetLevel(void)
{
    return this->active;
}

NullOutput::NullOutput(struct ConfigOutput* cfg, SignalProvider& prov)
{
    this->in = prov.FindOrAdd(cfg->SignalName);
    this->in->AddReceiver(this);
    this->newLevel = false;
    this->active = false;
    this->name = cfg->Name;
}

NullOutput::~NullOutput()
{}
