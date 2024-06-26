
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
    if (this->isDummy)
        return;

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

    if (this->isDummy)
        return "off";


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

string VoltageRegulatorSysfs::ReadState()
{
    string line;

    if (this->isDummy)
        return "disabled";

    ifstream infile(sysfsRoot / path("state"));
    getline(infile, line);
    infile.close();

    return line;
}

string VoltageRegulatorSysfs::ReadConsumerState()
{
    string line;

    if (this->isDummy)
        return "disabled";

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
// type "reg-userspace-consumer" or type "regulator-output"
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

                if ((line.find("reg-userspace-consumer") != string::npos) ||
                    (line.find("regulator-output") != string::npos))
                    return it->path().string();
            }
        }
        catch (exception e)
        {}
        it++;
    }
    return "";
}

VoltageRegulatorSysfs::VoltageRegulatorSysfs(struct ConfigRegulator* cfg, string root) :
    name(cfg->Name)
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
         log_debug("reg-userspace-consumer for regulator " +
                            cfg->Name + " not found in sysfs.");
        if (cfg->FallBackDummy) {
            log_debug("Fallback to dummy regulator for: " + cfg->Name);
            this->isDummy = true;
        }
    } else {
        log_debug("Consumer sysfs path of regulator " + cfg->Name + " is " +
                  consumerRoot);
        this->sysfsConsumerRoot = path(consumerRoot);
    }
}

VoltageRegulatorSysfs::~VoltageRegulatorSysfs()
{}
