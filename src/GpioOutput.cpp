
#include "GpioOutput.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string GpioOutput::Name(void)
{
    return this->chip.name() + "/" + this->line.name();
}

void GpioOutput::Apply(const int newLevel)
{
    if (newLevel != this->level)
    {
        this->level = newLevel;
        log_debug("output gpio " + this->Name() + " changed to " +
            ((this->level ^ this->activeLow) ? "1" : "0"));

        this->line.set_value(this->level);

        if (!this->DisableGpioOutCheck && this->line.get_value() != newLevel) {
            this->line.update();
            usleep(100000);
            if (this->line.get_value() != newLevel) {
                log_debug("GPIOSET_ERR: output gpio " + this->Name());
                log_sel("Failed to set gpio " + this->Name() + " to " + ((newLevel ^ this->activeLow) ? "1" : "0"),
                        "/xyz/openbmc_project/inventory/system/chassis/motherboard", true);
            }
        }
    }
}

void GpioOutput::Update(void)
{
    if (this->in->GetLevel() < 0)
        return;
    const int newLevel = this->in->GetLevel() ? 1 : 0;

    ioOutput->post([this, newLevel]() {
        this->Apply(newLevel);
    });
}

GpioOutput::GpioOutput(boost::asio::io_service *IoOutput, struct ConfigOutput* cfg, SignalProvider& prov) :
        ioOutput(IoOutput), level(-1), activeLow(cfg->ActiveLow)
{
    ::std::bitset<32> flags = 0;

    if (cfg->GpioChipName == "")
    {
        this->line = gpiod::find_line(cfg->Name);
        this->chip = this->line.get_chip();
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
    if (cfg->DisableBias)
        flags |= gpiod::line_request::FLAG_BIAS_DISABLE;
#endif
    ::gpiod::line_request requestOutput = {
        "pwrseqd", gpiod::line_request::DIRECTION_OUTPUT, flags};
    try
    {
        this->line.request(requestOutput);
    }
    catch (exception& e)
    {
        throw runtime_error("Failed to request gpio line " + cfg->GpioChipName +
                            " " + cfg->Name + ": " + e.what());
    }

    this->DisableGpioOutCheck = cfg->DisableGpioOutCheck;

    log_debug("using gpio " + this->Name() + " as output ");
}

GpioOutput::~GpioOutput()
{
    this->line.release();
}
