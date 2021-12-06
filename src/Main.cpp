
#include "ACPIStates.hpp"
#include "Config.hpp"
#include "Logging.hpp"
#include "SignalProvider.hpp"
#include "StateMachine.hpp"
#include "SysFsWatcher.hpp"

#include <popl.hpp>

#include <iostream>

using namespace std;
using namespace popl;

int _loglevel;

int main(int argc, const char* argv[])
{
    Config cfg;
    string dumpSignalsFolder;
    boost::asio::io_service io;
    SysFsWatcher* sysw;

    OptionParser op("Allowed options");
    auto help_option = op.add<Switch>("h", "help", "produce help message");
    auto config_option = op.add<Value<string>>(
        "c", "config", "Path to configuration file/folder.");
    auto dump_signals_option = op.add<Value<string>>(
        "d", "dump_signals_folder", "Path to dump signal.txt [DEBUGGING ONLY]");
    auto verbose = op.add<Switch>("v", "verbose",
                                  "Enable verbose logging [DEBUGGING ONLY]");
    auto extra = op.add<Switch>(
        "e", "extra_verbose", "Enable extra verbose logging [DEBUGGING ONLY]");
    auto quiet =
        op.add<Switch>("q", "quiet", "Be quiet and don't log any errors");

    _loglevel = 1;

    log_info("Starting " + string(argv[0]) + " ....");

    try
    {
        op.parse(argc, argv);

        // print auto-generated help message
        if (help_option->count() == 1)
        {
            cout << op.help() << endl;
            return 0;
        }
        else if (help_option->count() == 2)
        {
            cout << op.help(Attribute::advanced) << endl;
            return 0;
        }
        else if (help_option->count() > 2)
        {
            cout << op.help(Attribute::expert) << endl;
            return 0;
        }

        // Set loglevel
        if (quiet->is_set())
            _loglevel = 0;
        else if (verbose->is_set())
            _loglevel = 2;
        else if (extra->is_set())
            _loglevel = 3;

        if (!config_option->is_set() || config_option->value() == "")
        {
            log_err("Didn't specify a valid config file");
            log_err(op.help());
            return 1;
        }
        if (dump_signals_option->is_set())
            dumpSignalsFolder = dump_signals_option->value();

        try
        {
            cfg = LoadConfig(config_option->value());
        }
        catch (const exception& ex)
        {
            log_err("Failed to load config: " + string(ex.what()));
            return 1;
        }
    }
    catch (const popl::invalid_option& e)
    {
        log_err("Invalid Option Exception: " + string(e.what()));
        log_err("error:  ");
        if (e.error() == invalid_option::Error::missing_argument)
            log_err("missing_argument");
        else if (e.error() == invalid_option::Error::invalid_argument)
            log_err("invalid_argument");
        else if (e.error() == invalid_option::Error::too_many_arguments)
            log_err("too_many_arguments");
        else if (e.error() == invalid_option::Error::missing_option)
            log_err("missing_option");

        if (e.error() == invalid_option::Error::missing_option)
        {
            string option_name(e.option()->name(OptionName::short_name, true));
            if (option_name.empty())
                option_name = e.option()->name(OptionName::long_name, true);
            log_err("option: " + option_name);
        }
        else
        {
            log_err("option: " + e.option()->name(e.what_name()));
            log_err("value:  " + e.value());
        }
        return EXIT_FAILURE;
    }
    catch (const exception& e)
    {
        log_err("Exception: " + string(e.what()));
        return EXIT_FAILURE;
    }
    log_info("Loaded config files.");

    sysw = GetSysFsWatcher(io);
    try
    {
        SignalProvider signalprovider(cfg, dumpSignalsFolder);
        ACPIStates states(cfg, signalprovider, io);
        signalprovider.AddDriver(&states);
        StateMachine sm(cfg, signalprovider, io);

        log_info("Validating config ...");

        sm.Validate();

        log_info("Starting main loop.");

        sm.Run();
    }
    catch (const exception& ex)
    {
        log_err("Failed to use provided configuration: " + string(ex.what()));
        delete sysw;
        return 1;
    }

    delete sysw;
    return 0;
}
