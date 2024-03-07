#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

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
#include <chrono>
using namespace std::chrono;

#include "Logging.hpp"

static boost::mutex logging_lock;

static inline std::string timestamp()
{
  auto now = system_clock::now();
  auto timeT = system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&timeT), "%a %b %d %Y %T")
      << '.' << std::setfill('0') << std::setw(3)
      << duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
  return ss.str();
}

void log_debug(const std::string& s)
{
    boost::lock_guard<boost::mutex> guard(logging_lock);

    if (_loglevel > 1)
    {
#ifdef WITH_PHOSPHOR_LOGGING
        phosphor::logging::log<level::DEBUG>(s.c_str());
#endif
        std::cout << "[" << timestamp() << "]DBG : " << s << std::endl;
    }
}

void log_info(const std::string& s)
{
    boost::lock_guard<boost::mutex> guard(logging_lock);

    if (_loglevel > 0)
    {
#ifdef WITH_PHOSPHOR_LOGGING
        phosphor::logging::log<level::INFO>(s.c_str());
#endif
        std::cout << "[" << timestamp() << "]INFO : " << s << std::endl;
    }
}

void log_err(const std::string& s)
{
    boost::lock_guard<boost::mutex> guard(logging_lock);

#ifdef WITH_PHOSPHOR_LOGGING
    phosphor::logging::log<level::ERR>(s.c_str());
#endif
    std::cout << "[" << timestamp() << "]ERR : " << s << std::endl;
}

void log_sel(const std::string& data, const std::string& path, const bool& assert)
{
    boost::lock_guard<boost::mutex> guard(logging_lock);

#ifdef WITH_PHOSPHOR_LOGGING
    phosphor::logging::details::commit(data.c_str());
#endif
}
