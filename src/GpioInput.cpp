
#include "GpioInput.hpp"

#include "Logging.hpp"
#include "Signal.hpp"
#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string GpioInput::Name(void)
{
    return this->chip.name() + "/" + this->line.name();
}

// Poll needs to be called in case the async GPIO events aren't supported
void GpioInput::Poll(const boost::system::error_code& e)
{
    this->out->SetLevel(this->line.get_value() != 0);
}

// OnEvent is called by the async handler whenever an GPIO event has occured
void GpioInput::OnEvent(gpiod::line_event line_event)
{
    bool high = line_event.event_type == gpiod::line_event::RISING_EDGE;
    high ^= this->ActiveLow;

    log_debug("input gpio " + this->Name() + " changed to " +
              to_string(high ? 1 : 0));

    this->out->SetLevel(line_event.event_type ==
                        gpiod::line_event::RISING_EDGE);
}

// Acquire removes the interrupt from the GPIO
void GpioInput::Acquire(void)
{
    if (!this->gated)
    {
        return;
    }
    this->gpiodRequestInput = {"pwrseqd", gpiod::line_request::EVENT_BOTH_EDGES,
                               this->gpiodFlags};
    try
    {
        this->line.request(this->gpiodRequestInput);
    }
    catch (exception& e)
    {
        throw runtime_error("Failed to request gpio line " + this->chip.name() +
                            " " + this->line.name() + ": " + e.what());
    }
    int gpioLineFd = this->line.event_get_fd();
    if (gpioLineFd < 0)
    {
        string errMsg = "Failed to get fd for gpio line " + this->Name();
        throw runtime_error(errMsg);
    }

    this->streamDesc.assign(gpioLineFd);

    this->WaitForGPIOEvent();

    log_debug("using gpio " + this->Name() + " as input");

    // Read initial level once ready
    this->io->post([&] { this->out->SetLevel(this->line.get_value() != 0); });

    this->gated = false;
}

// Release removes the interrupt from the GPIO
void GpioInput::Release(void)
{
    bool newLevel;

    if (this->gated)
    {
        return;
    }
    this->streamDesc.cancel();
    this->streamDesc.release();
    log_debug("ignoring gpio " + this->Name() + " input");

    this->line.release();
    this->gated = true;

    if (this->GatedIdleHigh)
    {
        newLevel = true;
    }
    else if (this->GatedIdleLow)
    {
        newLevel = false;
    }

    newLevel ^= this->ActiveLow;

    log_debug("input gpio " + this->Name() + " reads as " +
              to_string(newLevel ? 1 : 0));

    this->out->SetLevel(newLevel);
}

void GpioInput::Update(void)
{
    if (this->enable->GetLevel() && this->gated)
    {
        this->Acquire();
    }
    else if (!this->enable->GetLevel() && !this->gated)
    {
        this->Release();
    }
}

// WaitForGPIOEvent async waits for an event on the open gpiod fd
void GpioInput::WaitForGPIOEvent(void)
{
    this->streamDesc.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [&](const boost::system::error_code ec) {
            gpiod::line_event line_event;
            if (ec)
            {
                return;
            }
            try
            {
                // Reading might fail if line was closed by Release()
                line_event = this->line.event_read();
            }
            catch (system_error& exc)
            {
                log_debug("GPIO line " + this->Name() + ": " + exc.what());
                return;
            }
            this->OnEvent(line_event);
            if (!this->gated)
            {
                this->WaitForGPIOEvent();
            }
        });
}

GpioInput::GpioInput(boost::asio::io_context& io, struct ConfigInput* cfg,
                     SignalProvider& prov) :
    io(&io),
    GatedIdleHigh(cfg->GatedIdleHigh), GatedIdleLow(cfg->GatedIdleLow),
    out(NULL), enable(NULL), streamDesc(io), ActiveLow(cfg->ActiveLow),
    gpiodFlags(0), gated(true)
{

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

    this->out = prov.FindOrAdd(cfg->SignalName);

    if (cfg->ActiveLow)
        this->gpiodFlags |= gpiod::line_request::FLAG_ACTIVE_LOW;
#ifdef WITH_GPIOD_PULLUPS
    if (cfg->PullDown)
        this->gpiodFlags |= gpiod::line_request::FLAG_BIAS_PULL_DOWN;
    if (cfg->PullUp)
        this->gpiodFlags |= gpiod::line_request::FLAG_BIAS_PULL_UP;
    if (cfg->DisableBias)
        this->gpiodFlags |= gpiod::line_request::FLAG_BIAS_DISABLE;
#endif

    this->gpiodRequestInput = {"pwrseqd", gpiod::line_request::EVENT_BOTH_EDGES,
                               this->gpiodFlags};

    // Test if line can be acquired
    try
    {
        this->line.request(this->gpiodRequestInput);
    }
    catch (exception& e)
    {
        throw runtime_error("Failed to request gpio line " + cfg->GpioChipName +
                            " " + cfg->Name + ": " + e.what());
    }
    this->line.release();

    if (!cfg->GateInput)
    {
        this->Acquire();
    }
    else
    {
        this->enable = prov.FindOrAdd(cfg->Name + "_On");
        this->enable->AddReceiver(this);

        if (this->GatedIdleHigh)
        {
            this->out->SetLevel(1);
        }
        else if (this->GatedIdleLow)
        {
            this->out->SetLevel(0);
        }
    }
}

vector<Signal*> GpioInput::Signals(void)
{
    vector<Signal*> vec;
    vec.push_back(this->out);
    return vec;
}

GpioInput::~GpioInput()
{
    this->line.release();
}
