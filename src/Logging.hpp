#pragma once

extern int _loglevel;
#ifdef WITH_PHOSPHOR_LOGGING
#include <phosphor-logging/log.hpp>
#endif
#include <iostream>

static inline void log_debug(const std::string& s)
{
    if (_loglevel > 1)
    {
#ifdef WITH_PHOSPHOR_LOGGING
        phosphor::logging::log<phosphor::logging::level::DEBUG>(s.c_str());
#endif
        std::cout << "DBG : " << s << std::endl;
    }
}

static inline void log_info(const std::string& s)
{
    if (_loglevel > 0)
    {
#ifdef WITH_PHOSPHOR_LOGGING
        phosphor::logging::log<phosphor::logging::level::INFO>(s.c_str());
#endif
        std::cout << "INFO : " << s << std::endl;
    }
}

static inline void log_err(const std::string& s)
{
#ifdef WITH_PHOSPHOR_LOGGING
    phosphor::logging::log<phosphor::logging::level::ERR>(s.c_str());
#endif
    std::cout << "ERR : " << s << std::endl;
}
