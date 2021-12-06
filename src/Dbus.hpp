#pragma once

#include "Config.hpp"

#include <boost/asio.hpp>

#ifdef WITH_SDBUSPLUSPLUS
#include <sdbusplus/asio/object_server.hpp>
#endif

using namespace std;

namespace dbus
{

enum class HostState
{
    running,
    off,
    transitionToOff,
    standby,
    quiesced,
    transitionToRunning,
    diagnosticMode,
};

static std::string getHostStateName(HostState state)
{
    switch (state)
    {
        case HostState::running:
            return "Running";
            break;
        case HostState::off:
            return "Off";
            break;
        case HostState::transitionToOff:
            return "Transition to Off";
            break;
        case HostState::standby:
            return "Standby";
            break;
        case HostState::quiesced:
            return "Quiesced";
            break;
        case HostState::transitionToRunning:
            return "Transition to Running";
            break;
        case HostState::diagnosticMode:
            return "Diagnostic Mode";
            break;
        default:
            return "unknown state: " + std::to_string(static_cast<int>(state));
            break;
    }
}

static constexpr std::string_view getHostState(const HostState state)
{
    switch (state)
    {
        case HostState::running:
            return "xyz.openbmc_project.State.Host.HostState.Running";
        case HostState::off:
            return "xyz.openbmc_project.State.Host.HostState.Off";
        case HostState::transitionToOff:
            return "xyz.openbmc_project.State.Host.HostState.TransitioningToOff";
        case HostState::standby:
            return "xyz.openbmc_project.State.Host.HostState.Standby";
        case HostState::quiesced:
            return "xyz.openbmc_project.State.Host.HostState.Quiesced";
        case HostState::transitionToRunning:
            return "xyz.openbmc_project.State.Host.HostState.TransitioningToRunning";
        case HostState::diagnosticMode:
            return "xyz.openbmc_project.State.Host.HostState.DiagnosticMode";
        default:
            return "";
            break;
    }
};
} // namespace dbus

// The Dbus class handles the DBUS interface
class Dbus
{
  public:
    Dbus(Config& cfg, boost::asio::io_service& io);
    ~Dbus();
    void SetHostState(const dbus::HostState);
    void SetChassisState(const bool IsOn);

    void RegisterRequestedHostTransition(
        const std::function<bool(const std::string& requested,
                                 std::string& resp)>& handler);

    void RegisterRequestedPowerTransition(
        const std::function<bool(const std::string& requested,
                                 std::string& resp)>& handler);
    void Initialize(void);

  private:
#ifdef WITH_SDBUSPLUSPLUS
    std::shared_ptr<sdbusplus::asio::connection> conn;

    sdbusplus::asio::object_server hostServer;
    sdbusplus::asio::object_server chassisServer;

    std::shared_ptr<sdbusplus::asio::dbus_interface> hostIface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> chassisIface;
#endif
};
