
#include "ACPIStates.hpp"
#include "Config.hpp"
#include "Logging.hpp"
#include "SignalProvider.hpp"
#include "StateMachine.hpp"
#include "Netlink.hpp"

#include <chrono>
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
        {"help",                  no_argument,       0, 'h'},
        {"config",                required_argument, 0, 'c'},
        {"dump_signals_folder",   required_argument, 0, 'd'},
        {"reg_error_inj_name",    required_argument, 0, 'r'},
        {"reg_error_inj_timeout", required_argument, 0, 't'},
        {"netlink_events",        no_argument,       0, 'n'},
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
        else if (long_options[i].name == "reg_error_inj_name")
            args += "Regulator name to inject an error after <x> seconds";
        else if (long_options[i].name == "reg_error_inj_timeout")
            args += "The number of seconds after the error should be injected";
        else if (long_options[i].name == "netlink_events")
            args += "Only dump regulator NETLINK events";

        args += "\n";
    }
    log_err(args);
}

int main(int argc, char * const argv[])
{
    Config cfg;
    string dumpSignalsFolder;
    boost::asio::io_service io;
    boost::asio::io_service IoOutput;
    int opt;
    int option_index = 0;
    int errorTimeout = 0;
    int netlinkEvents = 0;
    string config_option;
    std::time_t now = std::time(nullptr);
    boost::asio::deadline_timer timerTestErrorEvent(io);
    string errorRegulatorName;

    _loglevel = 1;

    if (argc <= 1) {
        log_err("No argument given.");
        help();
        return 1;
    }

    log_info("Starting " + string(argv[0]) + "....");

    try
    {
        opterr = 0;

        while ((opt = getopt_long(argc, argv, "hc:d:eqt:r:n", long_options, &option_index)) != -1 ) {
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
                case 't':
                    errorTimeout = atoi(optarg);
                    break;
                case 'r':
                    errorRegulatorName = string(optarg);
                    break;
                case 'n':
                    netlinkEvents = 1;
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

    std::thread OutputThread{[&IoOutput](){
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
            work_guard(IoOutput.get_executor());
        IoOutput.run();
    }};

    if (netlinkEvents) {
        NetlinkRegulatorEvents *netlink = GetNetlinkRegulatorEvents(io);
        for (auto it : cfg.Regulators)
        {
            netlink->Register(it.Name, [](std::string name, uint64_t events) {
                std::cout << name + ": Got NETLINK event: " + to_string(events);
            });
        }
        io.run();
        return 0;
    }

    try
    {
        SignalProvider signalprovider(cfg, dumpSignalsFolder);
        Dbus dbus(cfg, io);
        ACPIStates states(cfg, signalprovider, io, dbus, false);
        signalprovider.AddDriver(&states);
        StateMachine sm(cfg, signalprovider, io, IoOutput, dbus);

        log_info("Validating config ...");

        sm.Validate();
        if (errorRegulatorName != "" && errorTimeout > 0) {
            log_info("Starting error inject timer for " + errorRegulatorName + "...");
            timerTestErrorEvent.expires_from_now(boost::posix_time::seconds(errorTimeout));
            timerTestErrorEvent.async_wait([&sm, &errorRegulatorName](const boost::system::error_code& err) {
                sm.InjectRegulatorError(errorRegulatorName);
            });
        }

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
