
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

// Acquire adds the interrupt listener to the GPIO and releases it
void GpioInput::TestAcquire(void)
{
    this->line.request(this->gpiodRequestInput);
    this->line.release();
}

// Configures the line as Output and sets it to drive the line low
void GpioInput::SetOutputLow(void)
{
    ::gpiod::line_request requestOutput = {
                "pwrseqd", gpiod::line_request::DIRECTION_OUTPUT, 0};
    try
    {
        this->line.request(requestOutput);
    }
    catch (exception& e)
    {
        throw runtime_error("Failed to request gpio line " + this->chip.name() +
                    " " + this->line.name() + ": " + e.what());
    }
    this->line.set_value(0);
    log_debug("Set gpio " + this->Name() + " to output low");
}

// Acquire adds the interrupt listener to the GPIO
void GpioInput::Acquire(void)
{
    if (!this->gated)
    {
        return;
    }
    if (this->GatedOutputODLow) {
        this->line.release();
    }

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

    this->gated = false;

    // Read initial level once ready.
    // If Release() is called before initial level could be read this will throw
    // an exception.
    this->ioOutput->post([this] {
        int val;
        if (this->gated)
        {
            return;
        }

        try
        {
            val = this->line.get_value();
        }
        catch (system_error& exc)
        {
            log_debug("GPIO line " + this->Name() + ": " + exc.what());
            return;
        }

        this->io->post([this, val]() {
            this->out->SetLevel(val != 0);
            if (this->enabled)
                this->enabled->SetLevel(true);
        });
    });
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

    if (this->GatedOutputODLow) {
        this->SetOutputLow();
    }

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

    this->io->post([this, newLevel]() {
        this->out->SetLevel(newLevel);
        if (this->enabled)
            this->enabled->SetLevel(false);
    });
}

void GpioInput::Update(void)
{
    if (this->enable->GetLevel() && this->gated)
    {
        this->ioOutput->post([this]() {
            this->Acquire();
        });
    }
    else if (!this->enable->GetLevel() && !this->gated)
    {
        this->ioOutput->post([this]() {
            this->Release();
        });
    }
}

// WaitForGPIOEvent async waits for an event on the open gpiod fd
void GpioInput::WaitForGPIOEvent(void)
{
    this->streamDesc.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [this](const boost::system::error_code ec) {
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

GpioInput::GpioInput(boost::asio::io_context& io,
                     boost::asio::io_context& ioOutput,
                     struct ConfigInput* cfg,
                     SignalProvider& prov) :
    io(&io), ioOutput(&ioOutput),
    GatedIdleHigh(cfg->GatedIdleHigh), GatedIdleLow(cfg->GatedIdleLow),
    GatedOutputODLow(cfg->GatedOutputODLow),
    out(NULL), enable(NULL), enabled(NULL), streamDesc(io), ActiveLow(cfg->ActiveLow),
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

    if (!cfg->GateInput)
    {
        this->Acquire();
    }
    else
    {
        this->enable = prov.FindOrAdd(cfg->Name + "_On");
        this->enabled = prov.FindOrAdd(cfg->Name + "_Enabled");
        this->enable->AddReceiver(this);
        this->enabled->SetLevel(false);

        if (this->GatedIdleHigh)
        {
            this->out->SetLevel(this->ActiveLow ? false : true);
        }
        else if (this->GatedIdleLow)
        {
            this->out->SetLevel(this->ActiveLow ? true : false);
        }

        if (this->GatedOutputODLow) {
            this->SetOutputLow();
        } else {
            try
            {
                TestAcquire();
            }
            catch (exception& e)
            {
                throw runtime_error("Failed to request gpio line " + this->chip.name() +
                                " " + this->line.name() + ": " + e.what());
            }
        }
    }
}

vector<Signal*> GpioInput::Signals(void)
{
    vector<Signal*> vec;
    vec.push_back(this->out);
    if (this->enabled)
        vec.push_back(this->enabled);

    return vec;
}

GpioInput::~GpioInput()
{
    this->line.release();
}
