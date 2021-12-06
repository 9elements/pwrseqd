#include "Config.hpp"

#include "Logging.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <filesystem>
#include <iostream>

namespace fs = filesystem;

using namespace std;

namespace YAML
{
template <>
struct convert<ConfigOutput>
{

    static bool decode(const Node& node, ConfigOutput& c)
    {
        if (!node.IsMap())
        {
            return false;
        }

        c.OutputType = OUTPUT_TYPE_UNKNOWN;
        c.Name = "";
        c.SignalName = "";
        c.Description = "";
        c.GpioChipName = "";
        c.ActiveLow = false;
        c.OpenDrain = false;
        c.OpenSource = false;
        c.PullUp = false;
        c.PullDown = false;
        for (auto it : node)
        {
            string key = it.first.as<string>();

            if (key == "name")
            {
                c.Name = it.second.as<string>();
            }
            else if (key == "signal_name")
            {
                c.SignalName = it.second.as<string>();
            }
            else if (key == "description")
            {
                c.Description = it.second.as<string>();
            }
            else if (key == "gpio_chip_name")
            {
                c.GpioChipName = it.second.as<string>();
            }
            else if (key == "active_low")
            {
                c.ActiveLow = it.second.as<bool>();
            }
            else if (key == "open_drain")
            {
                c.OpenDrain = it.second.as<bool>();
            }
            else if (key == "open_source")
            {
                c.OpenSource = it.second.as<bool>();
            }
            else if (key == "pullup")
            {
                c.PullUp = it.second.as<bool>();
            }
            else if (key == "pulldown")
            {
                c.PullDown = it.second.as<bool>();
            }
            else if (key == "type")
            {
                string nameOfType = it.second.as<string>();
                if (nameOfType.compare("gpio") == 0)
                {
                    c.OutputType = OUTPUT_TYPE_GPIO;
                }
                else if (nameOfType.compare("null") == 0)
                {
                    c.OutputType = OUTPUT_TYPE_NULL;
                }
                else
                {
                    return false;
                }
            }
        }
        if (c.OutputType == OUTPUT_TYPE_UNKNOWN || c.Name == "" ||
            c.SignalName == "")
            return false;
        if (c.OpenDrain && c.OpenSource)
        {
            log_err(
                "Specified both, open-drain and open-source on the same GPIO");
            return false;
        }
#ifdef WITH_GPIOD_PULLUPS
        if (c.PullDown && c.PullUp)
        {
            log_err("Specified both, pull-up and pull-down on the same GPIO");
            return false;
        }
#else
        if (c.PullDown || c.PullUp)
        {
            log_err(
                "libgpiod has no pull-up and pull-down support. Please upgrade");
            return false;
        }
#endif
        return true;
    }
};

template <>
struct convert<ConfigInput>
{

    static bool decode(const Node& node, ConfigInput& c)
    {
        if (!node.IsMap())
        {
            return false;
        }

        c.InputType = INPUT_TYPE_UNKNOWN;
        c.Name = "";
        c.SignalName = "";
        c.Description = "";
        c.GpioChipName = "";
        c.ActiveLow = false;
        c.PullUp = false;
        c.PullDown = false;
        for (auto it : node)
        {
            string key = it.first.as<string>();

            if (key == "name")
            {
                c.Name = it.second.as<string>();
            }
            else if (key == "signal_name")
            {
                c.SignalName = it.second.as<string>();
            }
            else if (key == "description")
            {
                c.Description = it.second.as<string>();
            }
            else if (key == "gpio_chip_name")
            {
                c.GpioChipName = it.second.as<string>();
            }
            else if (key == "active_low")
            {
                c.ActiveLow = it.second.as<bool>();
            }
            else if (key == "pullup")
            {
                c.PullUp = it.second.as<bool>();
            }
            else if (key == "pulldown")
            {
                c.PullDown = it.second.as<bool>();
            }
            else if (key == "type")
            {
                string nameOfType = it.second.as<string>();
                if (nameOfType == "gpio")
                    c.InputType = INPUT_TYPE_GPIO;
                else if (nameOfType == "null")
                    c.InputType = INPUT_TYPE_NULL;
                else
                    return false;
            }
            else
            {
                log_info("Found unknown key '" + key + "' in unit " + c.Name);
            }
        }
        if (c.InputType == INPUT_TYPE_UNKNOWN || c.Name == "" ||
            c.SignalName == "")
            return false;
#ifdef WITH_GPIOD_PULLUPS
        if (c.PullDown && c.PullUp)
        {
            log_err("Specified both, pull-up and pull-down on the same GPIO");
            return false;
        }
#else
        if (c.PullDown || c.PullUp)
        {
            log_err(
                "libgpiod has no pull-up and pull-down support. Please upgrade");
            return false;
        }
#endif
        return true;
    }
};

template <>
struct convert<ConfigLogicOutput>
{

    static bool decode(const Node& node, ConfigLogicOutput& c)
    {
        if (!node.IsMap())
        {
            return false;
        }

        c.ActiveLow = false;
        c.SignalName = "";

        for (auto it : node)
        {
            string key = it.first.as<string>();
            if (key == "name")
                c.SignalName = it.second.as<string>();
            else if (key == "active_low")
                c.ActiveLow = it.second.as<bool>();
            else
                return false;
        }
        if (c.SignalName == "")
        {
            log_err("missing 'name' field");
            return false;
        }

        return true;
    }
};

template <>
struct convert<vector<ConfigLogicInput>>
{

    static bool decode(const Node& node, vector<ConfigLogicInput>& v)
    {
        if (!node.IsSequence())
        {
            return false;
        }
        for (int i = 0; i < node.size(); i++)
        {
            v.push_back(node[i].as<ConfigLogicInput>());
        }

        return true;
    }
};

template <>
struct convert<ConfigLogicInput>
{

    static bool decode(const Node& node, ConfigLogicInput& c)
    {
        if (!node.IsMap())
        {
            return false;
        }

        c.InputStableUsec = 0;
        c.Invert = false;
        c.SignalName = "";

        for (auto it : node)
        {
            string key = it.first.as<string>();
            if (key == "name")
                c.SignalName = it.second.as<string>();
            else if (key == "invert")
                c.Invert = it.second.as<bool>();
            else if (key == "input_stable_usec")
                c.InputStableUsec = it.second.as<int>();
            else
                return false;
        }
        if (c.SignalName == "")
        {
            log_err("missing 'name' field");
            return false;
        }

        return true;
    }
};

template <>
struct convert<ConfigLogic>
{

    static bool decode(const Node& node, ConfigLogic& c)
    {
        if (!node.IsMap())
        {
            return false;
        }

        YAML::Node key = node.begin()->first;
        YAML::Node value = node.begin()->second;
        c.Name = key.as<string>();
        c.AndThenOr = false;
        c.InvertFirstGate = false;
        c.DelayOutputUsec = 0;

        for (auto it : value)
        {
            string key = it.first.as<string>();

            if (key == "in")
            {
                YAML::Node inmap = it.second;
                if (inmap.IsMap())
                {
                    for (auto it2 : inmap)
                    {
                        string key2 = it2.first.as<string>();
                        if (key2 == "and")
                        {
                            c.AndSignalInputs =
                                it2.second.as<vector<ConfigLogicInput>>();
                        }
                        else if (key2 == "or")
                        {
                            c.OrSignalInputs =
                                it2.second.as<vector<ConfigLogicInput>>();
                        }
                        else if (key2 == "and_then_or")
                        {
                            c.AndThenOr = it2.second.as<bool>();
                        }
                        else if (key2 == "invert_first_gate")
                        {
                            c.InvertFirstGate = it2.second.as<bool>();
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
            }
            else if (key == "out")
            {
                c.Out = it.second.as<ConfigLogicOutput>();
            }
            else if (key == "delay_usec")
            {
                c.DelayOutputUsec = it.second.as<int>();
            }
            else
            {
                log_info("Found unknown key '" + key + "' in unit " + c.Name);
            }
        }
        if (c.Out.SignalName == "")
        {
            log_err("Unit " + c.Name + " doesn't have 'out' section");
            return false;
        }
        if ((!c.AndSignalInputs.size() && !c.OrSignalInputs.size()))
        {
            log_err("Unit " + c.Name + " doesn't have 'in' section");
            return false;
        }

        return true;
    }
};

template <>
struct convert<ConfigImmutable>
{

    static bool decode(const Node& node, ConfigImmutable& c)
    {
        if (!node.IsMap())
        {
            return false;
        }

        c.SignalName = "";
        bool levelFound = false;
        for (auto it : node)
        {
            string key = it.first.as<string>();

            if (key == "signal_name")
                c.SignalName = it.second.as<string>();
            else if (key == "value")
            {
                c.Level = it.second.as<bool>();
                levelFound = true;
            }
        }
        if (c.SignalName == "" || !levelFound)
            return false;

        return true;
    }
};

template <>
struct convert<ConfigRegulator>
{

    static bool decode(const Node& node, ConfigRegulator& c)
    {
        if (!node.IsMap())
        {
            return false;
        }

        c.Name = "";

        for (auto it : node)
        {
            string key = it.first.as<string>();

            if (key == "name")
            {
                c.Name = it.second.as<string>();
            }
            else if (key == "description")
            {
                // FIXME
            }
        }
        if (c.Name == "")
        {
            return false;
        }

        return true;
    }
};

template <>
struct convert<Config>
{

    static bool decode(const Node& node, Config& c)
    {
        if (!node.IsMap())
        {
            return true;
        }

        for (auto it : node)
        {
            string key = it.first.as<string>();

            if (key == "power_sequencer")
            {
                vector<ConfigLogic> newLogic =
                    it.second.as<vector<ConfigLogic>>();
                c.Logic.insert(c.Logic.end(), newLogic.begin(), newLogic.end());
            }
            else if (key == "inputs")
            {
                vector<ConfigInput> newInputs =
                    it.second.as<vector<ConfigInput>>();
                c.Inputs.insert(c.Inputs.end(), newInputs.begin(),
                                newInputs.end());
            }
            else if (key == "outputs")
            {
                vector<ConfigOutput> newOutputs =
                    it.second.as<vector<ConfigOutput>>();
                c.Outputs.insert(c.Outputs.end(), newOutputs.begin(),
                                 newOutputs.end());
            }
            else if (key == "floating_signals")
            {
                vector<string> signals = it.second.as<vector<string>>();
                c.FloatingSignals.insert(c.FloatingSignals.end(),
                                         signals.begin(), signals.end());
            }
            else if (key == "regulators")
            {
                vector<ConfigRegulator> regs =
                    it.second.as<vector<ConfigRegulator>>();
                c.Regulators.insert(c.Regulators.end(), regs.begin(),
                                    regs.end());
            }
            else if (key == "immutables")
            {
                vector<ConfigImmutable> imm =
                    it.second.as<vector<ConfigImmutable>>();
                c.Immutables.insert(c.Immutables.end(), imm.begin(), imm.end());
            }
        }

        return true;
    }
};
} // namespace YAML

static bool load(Config& cfg, string p)
{
    if ((boost::algorithm::ends_with(p, string(".yaml")) ||
         boost::algorithm::ends_with(p, string(".yml"))) &&
        fs::is_regular_file(p))
    {
        log_info("Loading YAML config " + p + "\n");
        YAML::Node root = YAML::LoadFile(p);

        Config newConfig = root.as<Config>();
        if (newConfig.Logic.size() > 0)
        {
            log_debug("merging " + to_string(newConfig.Logic.size()) +
                      " logic units into config\n");
            cfg.Logic.insert(cfg.Logic.end(), newConfig.Logic.begin(),
                             newConfig.Logic.end());
        }
        if (newConfig.Inputs.size() > 0)
        {
            log_debug("merging " + to_string(newConfig.Inputs.size()) +
                      " input units into config\n");
            cfg.Inputs.insert(cfg.Inputs.end(), newConfig.Inputs.begin(),
                              newConfig.Inputs.end());
        }
        if (newConfig.Outputs.size() > 0)
        {
            cfg.Outputs.insert(cfg.Outputs.end(), newConfig.Outputs.begin(),
                               newConfig.Outputs.end());
            log_debug("merging " + to_string(newConfig.Outputs.size()) +
                      " output units into config\n");
        }
        if (newConfig.Regulators.size() > 0)
        {
            cfg.Regulators.insert(cfg.Regulators.end(),
                                  newConfig.Regulators.begin(),
                                  newConfig.Regulators.end());
            log_debug("merging " + to_string(newConfig.Regulators.size()) +
                      " regulator units into config\n");
        }
        if (newConfig.Immutables.size() > 0)
        {
            cfg.Immutables.insert(cfg.Immutables.end(),
                                  newConfig.Immutables.begin(),
                                  newConfig.Immutables.end());
            log_debug("merging " + to_string(newConfig.Immutables.size()) +
                      " immutables units into config\n");
        }
        if (newConfig.ACPIStates.size() > 0)
        {
            cfg.ACPIStates.insert(cfg.ACPIStates.end(),
                                  newConfig.ACPIStates.begin(),
                                  newConfig.ACPIStates.end());
            log_debug("merging " + to_string(newConfig.ACPIStates.size()) +
                      " ACPI state units into config\n");
        }
        if (newConfig.FloatingSignals.size() > 0)
        {
            cfg.FloatingSignals.insert(cfg.FloatingSignals.end(),
                                       newConfig.FloatingSignals.begin(),
                                       newConfig.FloatingSignals.end());
            log_debug("merging " + to_string(newConfig.FloatingSignals.size()) +
                      " floating signals into config\n");
        }
        return true;
    }
    return false;
}

Config LoadConfig(string path)
{
    Config cfg;
    bool found = false;

    if (load(cfg, path))
        return cfg;

    for (auto& p : fs::recursive_directory_iterator(path))
        found |= load(cfg, p.path());

    if (!found)
        throw runtime_error("No config files found in " + path);

    return cfg;
}
