
#include "Dbus.hpp"

#include "Logging.hpp"

static std::string hostDbusName = "xyz.openbmc_project.State.Host";
static std::string chassisDbusName = "xyz.openbmc_project.State.Chassis";

using namespace dbus;

static constexpr std::string_view getChassisState(const bool isOn)
{
    if (isOn)
        return "xyz.openbmc_project.State.Chassis.PowerState.On";
    else
        return "xyz.openbmc_project.State.Chassis.PowerState.Off";
};

void Dbus::SetHostState(const dbus::HostState state)
{
#ifdef WITH_SDBUSPLUSPLUS
    this->hostIface->set_property("CurrentHostState",
                                  std::string(getHostState(state)));

    log_debug("DBUS CurrentHostState " + std::string(getHostState(state)));
#endif
}

void Dbus::SetChassisState(const bool IsOn)
{
#ifdef WITH_SDBUSPLUSPLUS
    this->chassisIface->set_property("CurrentPowerState",
                                     std::string(getChassisState(IsOn)));
    log_debug("DBUS CurrentPowerState " + std::string(getChassisState(IsOn)));

    this->chassisIface->register_property(
        "LastStateChangeTime",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() -
            std::chrono::system_clock::from_time_t(0))
            .count());
#endif
}

void Dbus::RegisterRequestedHostTransition(
    const std::function<bool(const std::string& requested, std::string& resp)>&
        handler)
{
#ifdef WITH_SDBUSPLUSPLUS

    this->hostIface->register_property(
        "RequestedHostTransition",
        std::string("xyz.openbmc_project.State.Host.Transition.Off"),
        [handler](const std::string& requested, std::string& resp) {
            return handler(requested, resp);
        });
#endif
}

void Dbus::RegisterRequestedPowerTransition(
    const std::function<bool(const std::string& requested, std::string& resp)>&
        handler)
{
#ifdef WITH_SDBUSPLUSPLUS
    this->chassisIface->register_property(
        "RequestedPowerTransition",
        std::string("xyz.openbmc_project.State.Chassis.Transition.Off"),
        [handler](const std::string& requested, std::string& resp) {
            return handler(requested, resp);
        });
#endif
}

void Dbus::Initialize(void)
{
#ifdef WITH_SDBUSPLUSPLUS
    this->hostIface->initialize();
    this->chassisIface->initialize();
#endif
}

Dbus::Dbus(Config& cfg, boost::asio::io_service& io)
#ifdef WITH_SDBUSPLUSPLUS
    :
    conn{std::make_shared<sdbusplus::asio::connection>(io)},
    hostServer{conn}, chassisServer
{
    conn
}

#endif
{
    string node = "0";
#ifdef WITH_SDBUSPLUSPLUS

    if (node == "0")
    {
        // Request all the dbus names
        conn->request_name(hostDbusName.c_str());
        conn->request_name(chassisDbusName.c_str());
    }

    hostDbusName += node;
    chassisDbusName += node;

    // Request all the dbus names
    conn->request_name(hostDbusName.c_str());
    conn->request_name(chassisDbusName.c_str());

    // Power Control Interface
    this->hostIface =
        hostServer.add_interface("/xyz/openbmc_project/state/host" + node,
                                 "xyz.openbmc_project.State.Host");

    this->hostIface->register_property(
        "CurrentHostState", std::string(getHostState(dbus::HostState::off)));

    // Chassis Control Interface
    this->chassisIface =
        chassisServer.add_interface("/xyz/openbmc_project/state/chassis" + node,
                                    "xyz.openbmc_project.State.Chassis");

    this->chassisIface->register_property("CurrentPowerState",
                                          std::string(getChassisState(false)));
    this->chassisIface->register_property(
        "LastStateChangeTime",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() -
            std::chrono::system_clock::from_time_t(0))
            .count());
#endif
}

Dbus::~Dbus()
{}