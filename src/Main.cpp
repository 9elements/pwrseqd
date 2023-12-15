
#include "ACPIStates.hpp"
#include "Config.hpp"
#include "Logging.hpp"
#include "SignalProvider.hpp"
#include "StateMachine.hpp"

#include <getopt.h>
#include <iostream>

using namespace std;

int _loglevel;

static struct option long_options[] =
{
        /* These options set a flag. */
        {"extra_verbose", no_argument,       &_loglevel, 3},
        {"verbose",       no_argument,       &_loglevel, 2},
        {"quiet",         no_argument,       &_loglevel, 0},
        /* These options don’t set a flag.
        We distinguish them by their indices. */
        {"help",                 no_argument,       0, 'h'},
        {"config",               required_argument, 0, 'c'},
        {"dump_signals_folder",  required_argument, 0, 'd'},
        {0, 0, 0, 0}
};

void help(void)
{
    int i;
    string args = "Allowed options:\n";
    for (i = 0; long_options[i].name; i++) {
        if (!long_options[i].flag && long_options[i].val) {
            args += "-";
            args += long_options[i].val;
        } else {
            args += "  ";
        }
        args += " --" + string(long_options[i].name) + " ";
        for (int j = string(long_options[i].name).length(); j < 30; j++)
           args += " ";

        if (long_options[i].name == "extra_verbose")
            args += "Enable extra verbose logging [DEBUGGING ONLY]";
        else if (long_options[i].name == "verbose")
            args += "Enable verbose logging [DEBUGGING ONLY]";
        else if (long_options[i].name == "quiet")
            args += "Disable logging";
        else if (long_options[i].name == "config")
            args += "Path to configuration file/folder.";
        else if (long_options[i].name == "dump_signals_folder")
            args += "Path to dump signal.txt [DEBUGGING ONLY]";
        else if (long_options[i].name == "help")
            args += "Produce this help message";

        args += "\n";
    }
    log_err(args);
}

int main(int argc, char * const argv[])
{
    Config cfg;
    string dumpSignalsFolder;
    boost::asio::io_service io;
    int opt;
    int option_index = 0;
    string config_option;

    _loglevel = 1;

    if (argc <= 1) {
        log_err("No argument given.");
        help();
        return 1;
    }

    log_info("Starting " + string(argv[0]) + " ....");

    try
    {
        opterr = 0;

        while ((opt = getopt_long(argc, argv, "hc:d:eq", long_options, &option_index)) != -1 ) {
            switch (opt) {
                case 0:
                    break;
                case 'h':
                    help();
                    return 0;
                case 'c':
                    config_option = string(optarg);
                    break;
                case 'd':
                    dumpSignalsFolder = string(optarg);
                    break;
                case '?':  // unknown option...
                    log_err(string("Unknown option: '") + char(optopt) + string("'!"));
                    help();
                    return 1;
            }
        }
        if (config_option == "")
        {
            log_err("Didn't specify a valid config file");
            help();
            return 1;
        }
        try
        {
            cfg = LoadConfig(config_option);
        }
        catch (const exception& ex)
        {
            log_err("Failed to load config: " + string(ex.what()));
            return 1;
        }
    }
    catch (const exception& e)
    {
        log_err("Exception: " + string(e.what()));
        return EXIT_FAILURE;
    }
    log_info("Loaded config files.");

    try
    {
        SignalProvider signalprovider(cfg, dumpSignalsFolder);
        Dbus dbus(cfg, io);
        ACPIStates states(cfg, signalprovider, io, dbus);
        signalprovider.AddDriver(&states);
        StateMachine sm(cfg, signalprovider, io, dbus);

        log_info("Validating config ...");

        sm.Validate();

        log_info("Starting main loop.");

        sm.Run();
    }
    catch (const exception& ex)
    {
        log_err("Failed to use provided configuration: " + string(ex.what()));
        return 1;
    }

    return 0;
}
