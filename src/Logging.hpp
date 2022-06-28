#pragma once

extern int _loglevel;
#ifdef WITH_PHOSPHOR_LOGGING
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Logging/SEL/error.hpp>

using namespace phosphor::logging;
using SELCreated =
    sdbusplus::xyz::openbmc_project::Logging::SEL::Error::Created;

#endif
#include <iostream>

static inline void log_debug(const std::string& s)
{
    if (_loglevel > 1)
    {
#ifdef WITH_PHOSPHOR_LOGGING
        phosphor::logging::log<level::DEBUG>(s.c_str());
#endif
        std::cout << "DBG : " << s << std::endl;
    }
}

static inline void log_info(const std::string& s)
{
    if (_loglevel > 0)
    {
#ifdef WITH_PHOSPHOR_LOGGING
        phosphor::logging::log<level::INFO>(s.c_str());
#endif
        std::cout << "INFO : " << s << std::endl;
    }
}

static inline void log_err(const std::string& s)
{
#ifdef WITH_PHOSPHOR_LOGGING
    phosphor::logging::log<level::ERR>(s.c_str());
#endif
    std::cout << "ERR : " << s << std::endl;
}


static inline void log_sel(const string& data, const std::string& path, const bool& assert) {
#ifdef WITH_PHOSPHOR_LOGGING
    using namespace xyz::openbmc_project::Logging::SEL;
    using namespace xyz::openbmc_project::Logging;
    // Generate System event record, BMC firmware
    report<SELCreated>(Entry::Level::Alert,
        Created::RECORD_TYPE(2), Created::GENERATOR_ID(0x41),
        Created::SENSOR_DATA(data.c_str()), Created::EVENT_DIR(assert),
        Created::SENSOR_PATH(path.c_str()));
#endif
}
