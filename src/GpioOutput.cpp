
#include "GpioOutput.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string GpioOutput::Name(void)
{
    return this->chip.name() + "/" + this->line.name();
}

void GpioOutput::Apply(void)
{
    if (this->newLevel != this->active)
    {
        this->active = this->newLevel;
        log_debug("output gpio " + this->Name() + " changed to " +
                  to_string(this->active ^ this->activeLow));
        this->line.set_value(this->newLevel);
    }
}

void GpioOutput::Update(void)
{
    this->newLevel = this->in->GetLevel();
}

GpioOutput::GpioOutput(struct ConfigOutput* cfg, SignalProvider& prov)
{
    ::std::bitset<32> flags = 0;

    if (cfg->GpioChipName == "")
    {
        for (auto& it : ::gpiod::make_chip_iter())
        {
            try
            {
                this->line = it.find_line(cfg->Name);
                this->chip = it;
                break;
            }
            catch (const ::system_error& exc)
            {
                continue;
            }
        }
    }
    else
    {
        this->chip.open(cfg->GpioChipName, gpiod::chip::OPEN_LOOKUP);
        this->line = this->chip.find_line(cfg->Name);
    }

    try
    {
        if (this->line.name() == "")
        {
            throw runtime_error("GPIO line " + cfg->Name + " not found");
        }
    }
    catch (logic_error& exc)
    {
        throw runtime_error("GPIO line " + cfg->Name + " not found");
    }

    this->in = prov.FindOrAdd(cfg->SignalName);
    this->in->AddReceiver(this);

    if (cfg->ActiveLow)
        flags |= gpiod::line_request::FLAG_ACTIVE_LOW;
    if (cfg->OpenDrain)
        flags |= gpiod::line_request::FLAG_OPEN_DRAIN;
    if (cfg->OpenSource)
        flags |= gpiod::line_request::FLAG_OPEN_SOURCE;
#ifdef WITH_GPIOD_PULLUPS
    if (cfg->PullDown)
        flags |= gpiod::line_request::FLAG_BIAS_PULL_DOWN;
    if (cfg->PullUp)
        flags |= gpiod::line_request::FLAG_BIAS_PULL_UP;
#endif
    ::gpiod::line_request requestOutput = {
        "pwrseqd", gpiod::line_request::DIRECTION_OUTPUT, flags};
    try
    {
        this->line.request(requestOutput);
    }
    catch (exception& e)
    {
        throw("Failed to request gpio line " + cfg->GpioChipName + " " +
              cfg->Name + ": " + e.what());
    }
    this->active = this->line.get_value() > 0;
    this->activeLow = cfg->ActiveLow;
    log_debug("using gpio " + this->Name() + " as output ");
}

GpioOutput::~GpioOutput()
{
    this->line.release();
}
