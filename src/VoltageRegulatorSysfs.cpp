
#include "VoltageRegulatorSysfs.hpp"
#include "Logging.hpp"

#include <filesystem>
#include <fstream>

using namespace std;
using namespace std::filesystem;

string VoltageRegulatorSysfs::EventsToString(const unsigned long events)
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

enum RegulatorStatus VoltageRegulatorSysfs::DecodeEvents(unsigned long events)
{
    if (events & REGULATOR_EVENT_FAILURE)
    {
        return ERROR;
    }
    /*
     * The event 'disable' and 'enable' do not reflect the status.
     * They happen when the consumer state changes.
     */
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

string VoltageRegulatorSysfs::StatusToString(const enum RegulatorStatus status)
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
        case UNDEFINED: return "UNDEFINED";
    }

    return "unknown";
}

string VoltageRegulatorSysfs::StateToString(const enum RegulatorState state)
{
    if (state == ENABLED)
        return "ENABLED";
    else
        return "DISABLED";
}

void VoltageRegulatorSysfs::SetState(const enum RegulatorState state)
{
    ofstream outfile(sysfsConsumerRoot / path("state"));
    if (state == ENABLED)
        log_debug("enabled regulator " + this->name);
    else
        log_debug("disabled regulator " + this->name);

    outfile << (state == ENABLED ? "enabled" : "disabled");
    outfile.close();
}

string VoltageRegulatorSysfs::ReadStatus()
{
    string line;
    ifstream infile(sysfsRoot / path("status"));
    getline(infile, line);
    infile.close();

    return line;
}

unsigned long VoltageRegulatorSysfs::DecodeRegulatorEvent(string state)
{
    if (!state.empty() && state[state.size() - 1] == '\n')
        state.pop_back();

    return stoul(state, 0, 0);
}

enum RegulatorStatus VoltageRegulatorSysfs::DecodeStatus(string state)
{
    static const struct
    {
        enum RegulatorStatus status;
        string str;
    } lookup[8] = {
        {OFF, "off"},         {ON, "on"},         {ERROR, "error"},
        {FAST, "fast"},       {NORMAL, "normal"}, {IDLE, "idle"},
        {STANDBY, "standby"}, {UNDEFINED, "undefined"},
    };

    for (int i = 0; i < 8; i++)
    {
        if (state == lookup[i].str || state == lookup[i].str + "\n")
        {
            return lookup[i].status;
        }
    }

    // Invalid state. Error to shut down platform.
    return INVALID;
}

enum RegulatorStatus VoltageRegulatorSysfs::DecodeStatus(void)
{
    return this->DecodeStatus(this->ReadStatus());
}

string VoltageRegulatorSysfs::ReadEvents()
{
    string line;
    ifstream infile(sysfsConsumerRoot / path("events"));
    getline(infile, line);
    infile.close();

    return line;
}

string VoltageRegulatorSysfs::ReadState()
{
    string line;
    ifstream infile(sysfsRoot / path("state"));
    getline(infile, line);
    infile.close();

    return line;
}

string VoltageRegulatorSysfs::ReadConsumerState()
{
    string line;
    ifstream infile(sysfsConsumerRoot / path("state"));
    getline(infile, line);
    infile.close();

    return line;
}

enum RegulatorState VoltageRegulatorSysfs::DecodeState(string state)
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
            return lookup[i].state;
        }
    }

    // Invalid state.
    return UNKNOWN;
}

enum RegulatorState VoltageRegulatorSysfs::DecodeState(void)
{
    return this->DecodeState(this->ReadConsumerState());
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

void VoltageRegulatorSysfs::RegisterStatusCallback(
    const std::function<void(const enum RegulatorStatus status)>&
        handler)
{
    this->watcher->Register(this->sysfsRoot / path("status"), [&, handler](filesystem::path p,
                                                         const char* data) {
        enum RegulatorStatus status = this->DecodeStatus(string(data));
        if (status == INVALID) {
            log_err(this->name + ": Got invalid status string '"+string(data)+"'");
        }
        log_debug(this->name + ": sysfsnotify on 'status': '"+StatusToString(status)+"'");

        handler(status);
    });
}

void VoltageRegulatorSysfs::RegisterStateCallback(
    const std::function<void(const enum RegulatorState state)>&
        handler)
{
    this->watcher->Register(this->sysfsConsumerRoot / path("state"),
                   [&, handler](filesystem::path p, const char* data) {
        enum RegulatorState state = this->DecodeState(string(data));
        log_debug(this->name + ": sysfsnotify on 'state': " + to_string(state));
        handler(state);
    });
}

bool VoltageRegulatorSysfs::RegisterEventCallback(
    const std::function<void(const unsigned long events)>&
        handler)
{
    if (!filesystem::exists(this->sysfsConsumerRoot / path("events")))
    {
        return false;
    }
    this->watcher->Register(
        this->sysfsConsumerRoot / path("events"),
        [&, handler](filesystem::path p, const char* data) {
            unsigned long events = this->DecodeRegulatorEvent(string(data));
            log_debug(this->name + ": sysfsnotify on 'events': (" + EventsToString(events) + ")");

            handler(events);
        });
    return true;
}

VoltageRegulatorSysfs::VoltageRegulatorSysfs(boost::asio::io_context& io,
                                             struct ConfigRegulator* cfg, string root) :
    watcher(GetSysFsWatcher(io)), name(cfg->Name)
{
    string consumerRoot;

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
}

VoltageRegulatorSysfs::~VoltageRegulatorSysfs()
{}
