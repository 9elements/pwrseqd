
#include "VoltageRegulator.hpp"

#include "Logging.hpp"
#include "SignalProvider.hpp"
#include "SysFsWatcher.hpp"

#include <filesystem>
#include <fstream>

using namespace std;
using namespace std::filesystem;


// Name returns the instance name
string VoltageRegulator::Name(void)
{
    return this->name;
}

void VoltageRegulator::Apply(void)
{
    const enum RegulatorState s = this->newLevel ? ENABLED : DISABLED;

    if (s != this->stateShadow)
    {
        this->timerStateCheck.cancel();

        this->SetState(s);

        // Setting the state might fail. Read current value from consumer.
        this->stateShadow = this->DecodeState(this->ReadConsumerState());
        if (s != this->stateShadow)
        {
            // Try again.
            this->SetState(s);
        }

        this->timerStateCheck.expires_from_now(boost::posix_time::microseconds(this->stateChangeTimeoutUsec));
        this->timerStateCheck.async_wait([&](const boost::system::error_code& err) {
            if (err != boost::asio::error::operation_aborted)
            {
                this->ConfirmStatusAfterTimeout();
            }
        });

        // This might glitch if regulator needs some time to disable
        this->enabled->SetLevel(s == ENABLED);
    }
}

vector<Signal*> VoltageRegulator::Signals(void)
{
    vector<Signal*> vec;

    vec.push_back(this->enabled);
    vec.push_back(this->powergood);
    vec.push_back(this->fault);

    return vec;
}

void VoltageRegulator::Update(void)
{
    this->newLevel = this->in->GetLevel();
}

string VoltageRegulator::EventsToString(const unsigned long events)
{
    string msg = "";
    if (events & REGULATOR_EVENT_UNDER_VOLTAGE)
        msg += "under-voltage ";
    if (events & REGULATOR_EVENT_OVER_CURRENT)
        msg += "over-current ";
    if (events & REGULATOR_EVENT_REGULATION_OUT)
        msg += "out of regulation ";
    if (events & REGULATOR_EVENT_OVER_TEMP)
        msg += "over-temperature ";
    if (events & REGULATOR_EVENT_FORCE_DISABLE)
        msg += "force-disabled ";
    if (events & REGULATOR_EVENT_DISABLE)
        msg += "disabled ";
    if (events & REGULATOR_EVENT_ENABLE)
        msg += "enabled ";
    return msg;
}

enum RegulatorStatus VoltageRegulator::DecodeEvents(unsigned long events)
{
    if (events & REGULATOR_EVENT_FAILURE)
    {
        return ERROR;
    }
    else if ((events & REGULATOR_EVENT_EN_DIS) == REGULATOR_EVENT_ENABLE)
    {
        return ON;
    }
    else if ((events & REGULATOR_EVENT_EN_DIS) == REGULATOR_EVENT_DISABLE)
    {
        return OFF;
    }

    return NOCHANGE;
}

string VoltageRegulator::StatusToString(const enum RegulatorStatus status)
{
    switch (status) {
        case OFF: return "OFF";
        case ON: return "ON";
        case ERROR: return "ERROR";
        case FAST: return "FAST";
        case NORMAL: return "NORMAL";
        case IDLE: return "IDLE";
        case STANDBY: return "STANDBY";
        case NOCHANGE: return "NOCHANGE";
        case INVALID: return "INVALID";
    }

    return "unknown";
}

string VoltageRegulator::StateToString(const enum RegulatorState state)
{
    if (state == ENABLED)
        return "ENABLED";
    else
        return "DISABLED";
}


void VoltageRegulator::ConfirmStatusAfterTimeout(void)
{
    bool failure = false;
    // First check state. Should never missmatch with a single regulator consumer.
    if (this->pendingNewLevel == DISABLED && this->stateShadow == ENABLED) {
        failure = true;
    } else if (this->pendingNewLevel == ENABLED && this->stateShadow == DISABLED) {
        failure = true;
    }
    // Now check status
    if (this->pendingNewLevel == ENABLED && this->statusShadow != ON) {
        failure = true;
    } else if (this->pendingNewLevel == DISABLED && this->statusShadow != OFF) {
        failure = true;
    }

    if (failure) {
        log_err(this->name + ": State didn't change after " + to_string(this->stateChangeTimeoutUsec) + " usec.");
        log_sel(this->name + ": State didn't change after " + to_string(this->stateChangeTimeoutUsec) + " usec.", this->sysfsRoot, true);

        this->pendingLevelChange = false;
        this->ApplyStatus(ERROR);
        // Something is wrong. Turn off regulator.
        this->SetState(DISABLED);
    }
}

void VoltageRegulator::ApplyStatus(enum RegulatorStatus status)
{
    // If status haven't changed just return
    // This can happen if a PRE_DISABLE or PRE_VOLTAGE_CHANGE is send
    if (this->statusShadow == status)
        return;
    if (status == NOCHANGE)
        return;

    log_debug(this->name + ": status changed to " + StatusToString(status) + ", was " + StatusToString(this->statusShadow));
    if (this->statusShadow == ERROR && status != ERROR) {
        log_err(this->name + ": Regulator recovered from error, new state is: " + StatusToString(status));
        log_sel(this->name + ": Regulator recovered from error, new state is: " + StatusToString(status), "/xyz/openbmc_project/inventory/system/chassis/motherboard", false);
    }
    this->statusShadow = status;

    if (status == OFF)
    {
        this->enabled->SetLevel(false);
        this->powergood->SetLevel(false);
        this->fault->SetLevel(false);
    }
    else if (status == ERROR)
    {
        // Errors might get cleared by interrupt handlers before we can
        // read them...
        this->enabled->SetLevel(true);
        this->powergood->SetLevel(false);
        this->fault->SetLevel(true);
    }
    else if (status == ON)
    {
        this->enabled->SetLevel(true);
        this->powergood->SetLevel(true);
        this->fault->SetLevel(false);
    }
    if (this->pendingLevelChange) {
        if (this->pendingNewLevel == ENABLED && status == ON)
            this->pendingLevelChange = false;
        else if (this->pendingNewLevel == DISABLED && status == OFF)
            this->pendingLevelChange = false;
        else {
            log_err(this->name + ": Got unexpected regulator status! Requested state " +
                StateToString(this->pendingNewLevel) + ", got status " + StatusToString(status));
        }
    } else {
        log_err(this->name + ": Got unexpected regulator status! Requested no state change, got status " + StatusToString(status));
        log_sel(this->name + ": Got unexpected regulator status! Requested no state change, got status: " + StatusToString(status), "/xyz/openbmc_project/inventory/system/chassis/motherboard", true);
        // Attempting to change it back is pointless.
        // The power sequencing logic will likely shutdown the system anyways.
    }
}

void VoltageRegulator::SetState(const enum RegulatorState state)
{
    ofstream outfile(sysfsConsumerRoot / path("state"));
    if (state == ENABLED)
        log_debug("enabled regulator " + this->name);
    else
        log_debug("disabled regulator " + this->name);

    outfile << (state == ENABLED ? "enabled" : "disabled");
    outfile.close();
    this->pendingLevelChange = true;
    this->pendingNewLevel = state;
}

string VoltageRegulator::ReadStatus()
{
    string line;
    ifstream infile(sysfsRoot / path("status"));
    getline(infile, line);
    infile.close();

    return line;
}

unsigned long VoltageRegulator::DecodeRegulatorEvent(string state)
{
    if (!state.empty() && state[state.size() - 1] == '\n')
        state.pop_back();

    return stoul(state, 0, 0);
}

enum RegulatorStatus VoltageRegulator::DecodeStatus(string state)
{
    static const struct
    {
        enum RegulatorStatus status;
        string str;
    } lookup[7] = {
        {OFF, "off"},         {ON, "on"},         {ERROR, "error"},
        {FAST, "fast"},       {NORMAL, "normal"}, {IDLE, "idle"},
        {STANDBY, "standby"},
    };

    for (int i = 0; i < 7; i++)
    {
        if (state == lookup[i].str || state == lookup[i].str + "\n")
        {
            log_debug("regulator " + this->name + " status is " +
                      lookup[i].str);

            return lookup[i].status;
        }
    }

    // Invalid state. Error to shut down platform.
    return INVALID;
}

string VoltageRegulator::ReadEvents()
{
    string line;
    ifstream infile(sysfsConsumerRoot / path("events"));
    getline(infile, line);
    infile.close();

    return line;
}

string VoltageRegulator::ReadState()
{
    string line;
    ifstream infile(sysfsRoot / path("state"));
    getline(infile, line);
    infile.close();

    return line;
}

string VoltageRegulator::ReadConsumerState()
{
    string line;
    ifstream infile(sysfsConsumerRoot / path("state"));
    getline(infile, line);
    infile.close();

    return line;
}

enum RegulatorState VoltageRegulator::DecodeState(string state)
{
    static const struct
    {
        enum RegulatorState state;
        string str;
    } lookup[3] = {
        {ENABLED, "enabled"},
        {DISABLED, "disabled"},
        {UNKNOWN, "unknown"},
    };

    for (int i = 0; i < 3; i++)
    {
        if (state == lookup[i].str || state == lookup[i].str + "\n")
        {
            log_debug("regulator " + this->name + " state is " + lookup[i].str);

            return lookup[i].state;
        }
    }

    // Invalid state.
    return UNKNOWN;
}

static string SysFsRootDirByName(string name)
{
    path root("/sys/class/regulator");
    directory_iterator it{root};
    while (it != directory_iterator{})
    {
        path p = *it / path("name");
        try
        {
            ifstream infile(p);
            if (infile.is_open())
            {
                string line;
                getline(infile, line);
                infile.close();

                if (line.compare(name) == 0)
                    return it->path().string();
            }
        }
        catch (exception e)
        {}
        it++;
    }
    return "";
}

// SysFsConsumerDir returns the sysfs path to the first consumer that is of
// type reg-userspace-consumer
static string SysFsConsumerDir(path root)
{
    directory_iterator it{root};
    while (it != directory_iterator{})
    {
        path p = *it / path("modalias");
        try
        {
            ifstream infile(p);
            if (infile.is_open())
            {
                string line;
                getline(infile, line);
                infile.close();

                if (line.find("reg-userspace-consumer") != string::npos)
                    return it->path().string();
            }
        }
        catch (exception e)
        {}
        it++;
    }
    return "";
}

void VoltageRegulator::Poll(void)
{
    // Regulators do not propagate their status changes through IRQs.
    // Only errors are passed through state change....
    this->timerPoll.expires_from_now(boost::posix_time::seconds(1));
    this->timerPoll.async_wait([&](const boost::system::error_code& err) {
        if (err != boost::asio::error::operation_aborted)
        {
            string statusText = this->ReadStatus();
            enum RegulatorStatus status = this->DecodeStatus(statusText);
            if (status == INVALID) {
                log_err(this->name + ": Got invalid status string '"+statusText+"'");
            }
            this->ApplyStatus(status);

            this->Poll();
        }
    });
}

VoltageRegulator::VoltageRegulator(boost::asio::io_context& io,
                                   struct ConfigRegulator* cfg,
                                   SignalProvider& prov, string root) :
    statusShadow(NOCHANGE), name(cfg->Name) , stateChangeTimeoutUsec(cfg->TimeoutUsec), timerStateCheck(io),
    pendingLevelChange(false), pendingNewLevel(DISABLED), timerPoll(io)
{
    string consumerRoot;
    this->in = prov.FindOrAdd(cfg->Name + "_On");
    this->in->AddReceiver(this);

    this->enabled = prov.FindOrAdd(cfg->Name + "_Enabled");
    this->fault = prov.FindOrAdd(cfg->Name + "_Fault");
    this->powergood = prov.FindOrAdd(cfg->Name + "_PowerGood");

    if (this->stateChangeTimeoutUsec == 0)
       this->stateChangeTimeoutUsec = DEFAULT_TIMEOUT_USEC;

    if (root == "")
        root = SysFsRootDirByName(cfg->Name);
    if (root == "")
    {
        throw runtime_error("Regulator " + cfg->Name + " not found in sysfs");
    }
    log_debug("Sysfs path of regulator " + cfg->Name + " is " + root);
    this->sysfsRoot = path(root);
    consumerRoot = SysFsConsumerDir(root);
    if (consumerRoot == "")
    {
        throw runtime_error("reg-userspace-consumer for regulator " +
                            cfg->Name + " not found in sysfs");
    }
    log_debug("Consumer sysfs path of regulator " + cfg->Name + " is " +
              consumerRoot);
    this->sysfsConsumerRoot = path(consumerRoot);

    // Set initial signal levels
    this->stateShadow = this->DecodeState(this->ReadConsumerState());

    // Silence warning when calling ApplyStatus
    this->pendingLevelChange = true;
    if (this->DecodeStatus(this->ReadStatus()) == OFF)
       this->pendingNewLevel = DISABLED;
    else if (this->DecodeStatus(this->ReadStatus()) == ON)
       this->pendingNewLevel = ENABLED;

    // Set current state
    this->ApplyStatus(this->DecodeStatus(this->ReadStatus()));

    // Register sysfs watcher
    SysFsWatcher* sysw = GetSysFsWatcher(io);
    sysw->Register(this->sysfsRoot / path("status"), [&](filesystem::path p,
                                                         const char* data) {
        enum RegulatorStatus status = this->DecodeStatus(string(data));
        if (status == INVALID) {
            log_err(this->name + ": Got invalid status string '"+string(data)+"'");
        }
        if (this->statusShadow == status)
        {
            io.post([&]() {
                string statusText = this->ReadStatus();
                enum RegulatorStatus status2 = this->DecodeStatus(statusText);
                if (status2 == INVALID) {
                    log_err(this->name + ": Got invalid status string '"+statusText+"'");
                }
                if (this->statusShadow != status)
                    log_debug(this->name + ": sysfsnotify on 'status'");

                this->ApplyStatus(status2);
            });
        } else {
            log_debug(this->name + ": sysfsnotify on 'status'");
            this->ApplyStatus(status);
        }
    });

    sysw->Register(this->sysfsConsumerRoot / path("state"),
                   [&](filesystem::path p, const char* data) {
                       this->stateShadow = this->DecodeState(string(data));
                       log_debug(this->name + ": sysfsnotify on 'state': " +
                                 to_string(this->stateShadow));
                       this->enabled->SetLevel(this->stateShadow == ENABLED);
                   });

    if (filesystem::exists(this->sysfsConsumerRoot / path("events")))
    {
        sysw->Register(
            this->sysfsConsumerRoot / path("events"),
            [&](filesystem::path p, const char* data) {
                enum RegulatorStatus status;
                unsigned long events = this->DecodeRegulatorEvent(string(data));
                log_debug(this->name +
                          ": sysfsnotify on 'events': " + string(data));

                status = this->DecodeEvents(events);
                if (status == ERROR)
                    log_sel(this->name + ": Regulator signaled error state(s): " + EventsToString(events), "/xyz/openbmc_project/inventory/system/chassis/motherboard", true);

                this->ApplyStatus(status);
            });
    }

    io.post([&]() {this->Poll();});
}

VoltageRegulator::~VoltageRegulator()
{}
