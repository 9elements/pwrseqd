
#include "LogOutput.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"

using namespace std;

// Name returns the instance name
string LogOutput::Name(void)
{
    return this->name;
}

void LogOutput::Apply(const int newLevel)
{
    if (newLevel == -1)
        return;
    int real_level = newLevel ^ this->activeLow;
    if (this->level == -1 && real_level == 0) {
        this->level = newLevel;
        return;
    }

    if (newLevel != this->level)
    {
        this->level = newLevel;
        log_debug("log " + this->Name() + " changed to " + (real_level ? "1" : "0"));

        if (real_level == 1)
            log_err("Monitor " + this->Name() + " (" + this->errorString + ") asserted");
        else
            log_info("Monitor " + this->Name() + " (" + this->errorString + ") de-asserted");
    }
}

void LogOutput::Update(void)
{
    if (this->in->GetLevel() < 0)
        return;
    if (this->enable && this->enable->GetLevel() <= 0)
        return;

    const int newLevel = this->in->GetLevel() ? 1 : 0;

    ioOutput->post([this, newLevel]() {
        this->Apply(newLevel);
    });
}

LogOutput::LogOutput(boost::asio::io_context& Io,
                     boost::asio::io_service *IoOutput,
                     struct ConfigOutput* cfg,
                     SignalProvider& prov) :
        name(cfg->Name), io(&Io), ioOutput(IoOutput), level(-1),
        activeLow(cfg->ActiveLow), enable(nullptr), errorString(cfg->Description)
{
    this->in = prov.FindOrAdd(cfg->SignalName);
    this->in->AddReceiver(this);

    this->enable = prov.Find(cfg->Name + "_On");
    if (this->enable) {
      this->enable->AddReceiver(this);
    }
}
