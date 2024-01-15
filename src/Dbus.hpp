#pragma once

#include "Config.hpp"

#include <boost/asio.hpp>
#include <boost/container/flat_map.hpp>

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

enum class HostTransition
{
    off,
    on,
};

enum class OSState
{
    inactive,
    standby,
};

enum class BootProgress
{
	Unspecified,
	PrimaryProcInit,
	MemoryInit,
	SecondaryProcInit,
	PCIInit,
	SystemSetup,
	SystemInitComplete,
	OSStart,
	OSRunning,
};

static constexpr std::string_view getOSState(const OSState state)
{
    switch (state)
    {
        case OSState::inactive:
            return "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Inactive";
        case OSState::standby:
            return "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Standby";
        default:
            return "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Inactive";
            break;
    }
};

static constexpr std::string_view getBootProgress(const BootProgress progress)
{
    switch (progress)
    {
        case BootProgress::Unspecified:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.Unspecified";
        case BootProgress::PrimaryProcInit:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.PrimaryProcInit";
        case BootProgress::MemoryInit:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.MemoryInit";
        case BootProgress::SecondaryProcInit:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.SecondaryProcInit";
        case BootProgress::PCIInit:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.PCIInit";
        case BootProgress::SystemSetup:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.SystemSetup";
        case BootProgress::SystemInitComplete:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.SystemInitComplete";
        case BootProgress::OSStart:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.OSStart";
        case BootProgress::OSRunning:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.OSRunning";
        default:
            return "xyz.openbmc_project.State.Boot.Progress.ProgressStages.Unspecified";
    }
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

static constexpr std::string_view getHostTransition(const HostTransition state)
{
    switch (state)
    {
        case HostTransition::off:
            return "xyz.openbmc_project.State.Host.Transition.Off";
        case HostTransition::on:
            return "xyz.openbmc_project.State.Host.Transition.On";
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
    void SetOSState(const dbus::OSState);
    void SetBootState(const dbus::BootProgress progress);
    void SetChassisState(const bool IsOn);
    void SetLEDState(string name, bool state);
    void RequestHostTransition(const dbus::HostTransition state);

    void RegisterItemPresentCallback(
        const std::string& inventoryPath,
        const std::function<void(const bool present)>&
            eventHandler);

    void GetItemPresentCallback(
        const std::string& inventoryPath,
        const std::function<void(const bool present)>&
            eventHandler);

    void RegisterRequestedHostTransition(
        const std::function<bool(const std::string& requested,
                                 std::string& resp)>& handler);

    void RegisterRequestedPowerTransition(
        const std::function<bool(const std::string& requested,
                                 std::string& resp)>& handler);
    void Initialize(void);

  private:
#ifdef WITH_SDBUSPLUSPLUS
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>> matches;

    std::shared_ptr<sdbusplus::asio::connection> conn;

    sdbusplus::asio::object_server hostServer;
    sdbusplus::asio::object_server bootServer;
    sdbusplus::asio::object_server chassisServer;
    sdbusplus::asio::object_server osServer;

    std::shared_ptr<sdbusplus::asio::dbus_interface> hostIface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> bootIface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> chassisIface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> osIface;

#endif
};
