#pragma once

// The authoritative contract lives in Code/plugins/STRPM and is shared with the
// companion plugins; the framework consumes the same header they do rather than
// keeping a private copy that could drift.
#include <STRPluginMessagingAPI/STRPluginMessagingAPI.h>

#include <Messages/NotifyPluginMessaging.h>

#include <TiltedCore/Stl.hpp>

#include <cstddef>
#include <cstdint>

struct World;

/**
 * @brief Native plugin-messaging transport for companion SKSE plugins.
 *
 * This is the in-process replacement for the binpatch bridge that the
 * standalone STRPluginMessagingAPI shipped: that bridge had to locate a
 * private logging anchor inside a packed third-party executable and hook its
 * chat path, because it had no other way to reach the session. This project
 * owns both ends of the protocol, so the payload travels as a real message and
 * none of that machinery is needed.
 *
 * Consumers keep using the same C ABI they already compile against, so no
 * plugin has to change: the STRPM entry point is exported from this module and
 * answers with a transport backed by the framework's own session.
 */
class PluginMessagingService
{
public:
    static PluginMessagingService& Get() noexcept;

    void Initialize(World& aWorld) noexcept;
    void Shutdown() noexcept;

    // ---- transport surface, called by the exported C ABI -------------------

    [[nodiscard]] STRPM::Result RegisterChannel(const char* acpChannel, STRPM::ReceiveCallback aCallback, void* apUserData, STRPM::ListenerHandle* apOutHandle) noexcept;
    [[nodiscard]] STRPM::Result UnregisterChannel(STRPM::ListenerHandle aHandle) noexcept;
    [[nodiscard]] STRPM::Result Send(const char* acpChannel, STRPM::Target aTarget, const void* apData, std::size_t aSize, std::uint32_t aFlags) noexcept;
    [[nodiscard]] STRPM::Result GetLocalConnectionId(STRPM::ConnectionID* apOutConnectionId) noexcept;
    [[nodiscard]] STRPM::Result SetLocalDisplayName(const char* acpDisplayName) noexcept;

    void SetLogCallback(STRPM::LogCallback aCallback, void* apUserData) noexcept;

    // ---- framework side ---------------------------------------------------

    void OnPluginMessage(const NotifyPluginMessaging& acMessage) noexcept;

private:
    PluginMessagingService() = default;
    ~PluginMessagingService() = default;

    PluginMessagingService(const PluginMessagingService&) = delete;
    PluginMessagingService& operator=(const PluginMessagingService&) = delete;

    struct Listener
    {
        TiltedPhoques::String Channel;
        STRPM::ReceiveCallback Callback{ nullptr };
        void* UserData{ nullptr };
    };

    void Log(const char* acpMessage) noexcept;

    // Registered channels are few and long lived; a vector keeps delivery
    // order stable and the lookup cost is irrelevant next to a network hop.
    TiltedPhoques::Vector<Listener> m_listeners;
    TiltedPhoques::Map<std::uint64_t, TiltedPhoques::String> m_channels;

    STRPM::LogCallback m_logCallback{ nullptr };
    void* m_logUserData{ nullptr };

    World* m_pWorld{ nullptr };
    std::uint64_t m_nextHandle{ 1 };
    TiltedPhoques::String m_localDisplayName;
};