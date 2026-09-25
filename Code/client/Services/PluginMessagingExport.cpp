// The C ABI companion plugins resolve at runtime.
//
// Consumers already look up these exact export names on STRPluginMessagingAPI.dll
// and fall back to LoadLibrary when the module is not present. Exporting the same
// names from the framework runtime means an existing plugin finds a native
// transport without being rebuilt, and the binpatch bridge is never involved.

#include <Services/PluginMessagingService.h>

#include <STRPluginMessagingAPI/STRPluginMessagingAPI.h>

#include <cstddef>
#include <cstdint>

namespace
{
STRPM::Result STRPM_CALL QueryInterface(std::uint32_t aRequestedVersion, const STRPM::Interface** appOutInterface) noexcept
{
    if (!appOutInterface)
        return STRPM::Result::kInvalidArgument;

    if (aRequestedVersion != STRPM::kInterfaceVersion)
        return STRPM::Result::kUnsupportedVersion;

    static const STRPM::Interface s_interface{
        STRPM::kInterfaceVersion,
        [](const char* acpChannel, STRPM::ReceiveCallback aCallback, void* apUserData, STRPM::ListenerHandle* apOutHandle) noexcept {
            return PluginMessagingService::Get().RegisterChannel(acpChannel, aCallback, apUserData, apOutHandle);
        },
        [](STRPM::ListenerHandle aHandle) noexcept { return PluginMessagingService::Get().UnregisterChannel(aHandle); },
        [](const char* acpChannel, STRPM::Target aTarget, const void* apData, std::size_t aSize, std::uint32_t aFlags) noexcept {
            return PluginMessagingService::Get().Send(acpChannel, aTarget, apData, aSize, aFlags);
        },
        [](STRPM::ConnectionID* apOutConnectionId) noexcept { return PluginMessagingService::Get().GetLocalConnectionId(apOutConnectionId); },
        [](STRPM::LogCallback aCallback, void* apUserData) noexcept { PluginMessagingService::Get().SetLogCallback(aCallback, apUserData); return STRPM::Result::kOk; },
        [](const char* acpDisplayName) noexcept { return PluginMessagingService::Get().SetLocalDisplayName(acpDisplayName); },
    };

    *appOutInterface = &s_interface;
    return STRPM::Result::kOk;
}

STRPM::Result STRPM_CALL QueryProxyResolver(std::uint32_t aRequestedVersion, const STRPM::ProxyResolverInterface** appOutInterface) noexcept
{
    if (!appOutInterface)
        return STRPM::Result::kInvalidArgument;

    // The native transport resolves peers by connection id, which the server
    // authenticates, so there is no FormID proxy table to consult. Reporting the
    // absence honestly lets a plugin fall back instead of reading a stale mapping.
    return STRPM::Result::kNotAvailable;
}
} // namespace

// Exported for the plugin-facing lookup. extern "C" keeps the names unmangled so
// GetProcAddress finds them regardless of the caller's toolchain.
extern "C" STRPM_EXPORT STRPM::Result STRPM_CALL STR_QueryPluginMessagingInterface(std::uint32_t aRequestedVersion, const STRPM::Interface** appOutInterface) noexcept
{
    return QueryInterface(aRequestedVersion, appOutInterface);
}

extern "C" STRPM_EXPORT STRPM::Result STRPM_CALL STR_QueryPluginMessagingProxyResolver(std::uint32_t aRequestedVersion, const STRPM::ProxyResolverInterface** appOutInterface) noexcept
{
    return QueryProxyResolver(aRequestedVersion, appOutInterface);
}
STRPM::Result STRPM_CALL QueryDiagnostics(std::uint32_t aRequestedVersion, const STRPM::DiagnosticsInterface** appOutInterface) noexcept
{
    if (!appOutInterface)
        return STRPM::Result::kInvalidArgument;

    if (aRequestedVersion != STRPM::kDiagnosticsVersion)
        return STRPM::Result::kUnsupportedVersion;

    // Only the fields this transport actually knows are filled in; the rest stay
    // at their zero defaults rather than being invented, because a plugin may
    // show these values to a user.
    static const STRPM::DiagnosticsInterface s_diagnostics{
        STRPM::kDiagnosticsVersion,
        [](STRPM::RuntimeStatus* apOutStatus) noexcept -> STRPM::Result {
            if (!apOutStatus)
                return STRPM::Result::kInvalidArgument;

            apOutStatus->version = STRPM::kDiagnosticsVersion;
            apOutStatus->activeBackend = STRPM::RuntimeBackend::kStrBridge;
            apOutStatus->configuredBackendMode = STRPM::RuntimeBackendMode::kAuto;
            apOutStatus->strBridgeAvailable = 1;
            apOutStatus->strBridgeActive = 1;
            return STRPM::Result::kOk;
        },
    };

    *appOutInterface = &s_diagnostics;
    return STRPM::Result::kOk;
}

// The STRPM facade resolves its transport through this entry point, so the
// framework answers it directly instead of shipping a separate bridge module
// that would have to find and patch the runtime from the outside.
STRPM::Result STRPM_CALL QueryTransport(std::uint32_t aRequestedVersion, const STRPM::TransportInterface** appOutInterface) noexcept
{
    if (!appOutInterface)
        return STRPM::Result::kInvalidArgument;

    if (aRequestedVersion != STRPM::kTransportInterfaceVersion)
        return STRPM::Result::kUnsupportedVersion;

    static const STRPM::TransportInterface s_transport{
        STRPM::kTransportInterfaceVersion,
        [](STRPM::ReceiveCallback aCallback, void* apUserData) noexcept -> STRPM::Result {
            // The facade hands its dispatcher over here and expects to receive
            // payloads through it, so the callback is retained rather than
            // dropped - the framework already owns the session, but the facade
            // still has to be given the bytes.
            return PluginMessagingService::Get().StartTransport(aCallback, apUserData);
        },
        []() noexcept -> STRPM::Result { return PluginMessagingService::Get().StopTransport(); },
        [](const char* acpChannel, STRPM::Target aTarget, const void* apData, std::size_t aSize, std::uint32_t aFlags) noexcept {
            return PluginMessagingService::Get().Send(acpChannel, aTarget, apData, aSize, aFlags);
        },
        [](STRPM::ConnectionID* apOutConnectionId) noexcept { return PluginMessagingService::Get().GetLocalConnectionId(apOutConnectionId); },
        [](const char* acpDisplayName) noexcept { return PluginMessagingService::Get().SetLocalDisplayName(acpDisplayName); },
    };

    *appOutInterface = &s_transport;
    return STRPM::Result::kOk;
}
} // namespace

extern "C" STRPM_EXPORT STRPM::Result STRPM_CALL STR_QueryPluginMessagingDiagnostics(std::uint32_t aRequestedVersion, const STRPM::DiagnosticsInterface** appOutInterface) noexcept
{
    return QueryDiagnostics(aRequestedVersion, appOutInterface);
}

extern "C" STRPM_EXPORT STRPM::Result STRPM_CALL STRPM_QueryTransportInterface(std::uint32_t aRequestedVersion, const STRPM::TransportInterface** appOutInterface) noexcept
{
    return QueryTransport(aRequestedVersion, appOutInterface);
}