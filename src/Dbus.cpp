
#include "Dbus.hpp"

#include "Logging.hpp"

namespace led
{
const static constexpr char* busname = "xyz.openbmc_project.LED.GroupManager";
const static constexpr char* prefix = "/xyz/openbmc_project/led/groups/";
const static constexpr char* interface = "xyz.openbmc_project.Led.Group";
const static constexpr char* property = "Asserted";
} // namespace led

namespace chassis
{
const static constexpr char* busname = "xyz.openbmc_project.State.Chassis";
} // namespace chassis

namespace power
{
const static constexpr char* busname = "xyz.openbmc_project.State.Host";
} // namespace power

namespace properties
{
const static constexpr char* interface = "org.freedesktop.DBus.Properties";
const static constexpr char* get = "Get";
const static constexpr char* set = "Set";
} // namespace properties

namespace inventory
{
const static constexpr char* busname = "xyz.openbmc_project.Inventory.Manager";
const static constexpr char* interface = "xyz.openbmc_project.Inventory.Item";
const static constexpr char* property = "Present";
const static constexpr char* prefix = "/xyz/openbmc_project/inventory/";
} // namespace inventory

namespace os
{
const static constexpr char* busname = "xyz.openbmc_project.State.OperatingSystem.Status";
}
using namespace dbus;

static constexpr std::string_view getChassisState(const bool isOn)
{
    if (isOn)
        return "xyz.openbmc_project.State.Chassis.PowerState.On";
    else
        return "xyz.openbmc_project.State.Chassis.PowerState.Off";
};

void Dbus::RegisterItemPresentCallback(
    const std::string& inventoryPath,
    const std::function<void(const bool present)>&
        eventHandler)
{
#ifdef WITH_SDBUSPLUSPLUS
    auto match = std::make_unique<sdbusplus::bus::match::match>(
            static_cast<sdbusplus::bus::bus&>(*conn),
            "type='signal',member='PropertiesChanged',interface='org."
            "freedesktop.DBus.Properties', path_namespace='" +
                std::string(inventory::prefix) + inventoryPath +
                "',arg0='" + inventory::interface + "'",
            [eventHandler](sdbusplus::message::message& message) {
                if (message.is_method_error())
                {
                    log_err("callback method error\n");
                    return;
                }
  
                boost::container::flat_map<std::string, std::variant<bool>> values;
                std::string objectName;
                message.read(objectName, values);
                const auto findValue = values.find("Present");
                if (findValue != values.end())
                {
                    eventHandler(std::get<bool>(findValue->second));
                }
            });
    matches.emplace_back(std::move(match));
#endif
}

void Dbus::GetItemPresentCallback(
    const std::string& inventoryPath,
    const std::function<void(const bool present)>&
        eventHandler)
{
#ifdef WITH_SDBUSPLUSPLUS
    conn->async_method_call(
        [eventHandler](boost::system::error_code ec,
            const std::variant<bool>& state) {
            if (ec)
            {
                log_err("error getting presence status: " + ec.message() + "\n");
                return;
            }
            eventHandler(std::get<bool>(state));
        },
        inventory::busname, std::string(inventory::prefix) + inventoryPath, properties::interface, properties::get,
        inventory::interface, inventory::property);
#endif
}

void Dbus::SetLEDState(string name, bool state)
{
#ifdef WITH_SDBUSPLUSPLUS
        conn->async_method_call(
            [name](const boost::system::error_code ec) {
                if (ec)
                {
                    log_err("Failed to set LED " + name + ": " + ec.message() +"\n");
                }
            },
            led::busname, std::string(led::prefix) + name, properties::interface,
            properties::set, led::interface, led::property,
            std::variant<bool>(state));
#endif
}

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

void Dbus::SetOSState(const dbus::OSState state)
{
#ifdef WITH_SDBUSPLUSPLUS
    this->osIface->set_property("OperatingSystemState",
                                std::string(getOSState(state)));

    log_debug("DBUS CurrentOSState " + std::string(getOSState(state)));
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
    this->osIface->initialize();
#endif
}

Dbus::Dbus(Config& cfg, boost::asio::io_service& io)
#ifdef WITH_SDBUSPLUSPLUS
    :
    conn{std::make_shared<sdbusplus::asio::connection>(io)},
    hostServer{conn}, chassisServer{conn}, osServer{conn}

#endif
{
    string node = "0";
#ifdef WITH_SDBUSPLUSPLUS

    if (node == "0")
    {
        // Request all the dbus names
        conn->request_name(power::busname);
        conn->request_name(chassis::busname);
    }

    // Request all the dbus names
    conn->request_name(std::string(power::busname).append(node).c_str());
    conn->request_name(std::string(chassis::busname).append(node).c_str());

    // OS State Interface
    this->osIface = osServer.add_interface(
        "/xyz/openbmc_project/state/os",
        os::busname);
    this->osIface->register_property(
        "OperatingSystemState",
        std::string(getOSState(dbus::OSState::inactive)));

    // Power Control Interface
    this->hostIface =
        hostServer.add_interface("/xyz/openbmc_project/state/host" + node,
                                 power::busname);

    this->hostIface->register_property(
        "CurrentHostState", std::string(getHostState(dbus::HostState::off)));

    // Chassis Control Interface
    this->chassisIface =
        chassisServer.add_interface("/xyz/openbmc_project/state/chassis" + node,
                                    chassis::busname);

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