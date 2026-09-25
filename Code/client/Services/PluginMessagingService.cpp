#include <World.h>

#include <Services/PluginMessagingService.h>
#include <Services/TransportService.h>

#include <Messages/PluginMessagingRequest.h>

#include <spdlog/spdlog.h>

#include <cstring>
#include <mutex>

namespace
{
// The registered channel list is touched from the game thread (plugin
// callbacks) and from the network thread (incoming payloads), so every access
// goes through this lock. Delivery happens on whichever thread received the
// message; the contract tells plugins to marshal to the game thread before
// touching game state, which is the same rule the framework follows internally.
std::mutex g_pluginMessagingMutex;
}

PluginMessagingService& PluginMessagingService::Get() noexcept
{
    static PluginMessagingService s_instance;
    return s_instance;
}

void PluginMessagingService::Initialize(World& aWorld) noexcept
{
    m_pWorld = &aWorld;
}

void PluginMessagingService::Shutdown() noexcept
{
    std::scoped_lock lock(g_pluginMessagingMutex);
    m_listeners.clear();
    m_pWorld = nullptr;
}

void PluginMessagingService::Log(const char* acpMessage) noexcept
{
    if (m_logCallback)
        m_logCallback(acpMessage, m_logUserData);
    else
        spdlog::info("STRPM: {}", acpMessage);
}

STRPM::Result PluginMessagingService::RegisterChannel(const char* acpChannel, STRPM::ReceiveCallback aCallback, void* apUserData, STRPM::ListenerHandle* apOutHandle) noexcept
{
    if (!acpChannel || !aCallback)
        return STRPM::Result::kInvalidArgument;

    const auto length = std::strlen(acpChannel);
    if (length == 0 || length > STRPM::kMaxChannelLength)
        return STRPM::Result::kInvalidArgument;

    std::scoped_lock lock(g_pluginMessagingMutex);

    for (const auto& listener : m_listeners)
    {
        if (listener.Channel == acpChannel)
            return STRPM::Result::kChannelAlreadyRegistered;
    }

    const auto handle = m_nextHandle++;
    m_listeners.push_back(Listener{ acpChannel, aCallback, apUserData });
    m_channels[handle] = acpChannel;

    if (apOutHandle)
        apOutHandle->value = handle;

    return STRPM::Result::kOk;
}

STRPM::Result PluginMessagingService::UnregisterChannel(STRPM::ListenerHandle aHandle) noexcept
{
    std::scoped_lock lock(g_pluginMessagingMutex);

    const auto it = m_channels.find(aHandle.value);
    if (it == m_channels.end())
        return STRPM::Result::kChannelNotRegistered;

    const auto& channel = it->second;
    for (auto listener = m_listeners.begin(); listener != m_listeners.end(); ++listener)
    {
        if (listener->Channel == channel)
        {
            m_listeners.erase(listener);
            break;
        }
    }

    m_channels.erase(it);
    return STRPM::Result::kOk;
}

STRPM::Result PluginMessagingService::Send(const char* acpChannel, STRPM::Target aTarget, const void* apData, std::size_t aSize, std::uint32_t aFlags) noexcept
{
    if (!acpChannel || (aSize > 0 && !apData))
        return STRPM::Result::kInvalidArgument;

    const auto length = std::strlen(acpChannel);
    if (length == 0 || length > STRPM::kMaxChannelLength)
        return STRPM::Result::kInvalidArgument;

    if (aSize > STRPM::kMaxPayloadBytes)
        return STRPM::Result::kPayloadTooLarge;

    if (!m_pWorld)
        return STRPM::Result::kNotAvailable;

    auto& transport = m_pWorld->GetTransport();
    if (!transport.IsOnline())
        return STRPM::Result::kNotConnected;

    PluginMessagingRequest request{};
    request.Channel = acpChannel;

    switch (aTarget.kind)
    {
    case STRPM::TargetKind::kServer:
        request.TargetKind = PluginMessagingRequest::Target::kServer;
        break;
    case STRPM::TargetKind::kHost:
        request.TargetKind = PluginMessagingRequest::Target::kHost;
        break;
    case STRPM::TargetKind::kPlayer:
        request.TargetKind = PluginMessagingRequest::Target::kPlayer;
        request.TargetConnectionId = aTarget.connectionID;
        break;
    case STRPM::TargetKind::kAllPlayers:
    default:
        request.TargetKind = PluginMessagingRequest::Target::kAllPlayers;
        break;
    }

    if (aSize > 0)
        request.PluginData.assign(static_cast<const std::uint8_t*>(apData), static_cast<const std::uint8_t*>(apData) + aSize);

    // The framework's own transport carries the payload; no separate socket and
    // no chat envelope, so nothing here can surface as a chat line.
    if (!transport.Send(request))
        return STRPM::Result::kTransportError;

    return STRPM::Result::kOk;
}

STRPM::Result PluginMessagingService::GetLocalConnectionId(STRPM::ConnectionID* apOutConnectionId) noexcept
{
    if (!apOutConnectionId)
        return STRPM::Result::kInvalidArgument;

    if (!m_pWorld)
        return STRPM::Result::kNotAvailable;

    auto& transport = m_pWorld->GetTransport();
    if (!transport.IsOnline())
        return STRPM::Result::kNotConnected;

    // Plugins address peers by the identifier the server uses on the wire, so
    // the local player id is the value they must hand back in a Target.
    *apOutConnectionId = static_cast<STRPM::ConnectionID>(transport.GetLocalPlayerId());
    return STRPM::Result::kOk;
}

STRPM::Result PluginMessagingService::SetLocalDisplayName(const char* acpDisplayName) noexcept
{
    if (!acpDisplayName)
        return STRPM::Result::kInvalidArgument;

    std::scoped_lock lock(g_pluginMessagingMutex);
    m_localDisplayName = acpDisplayName;
    return STRPM::Result::kOk;
}

void PluginMessagingService::SetLogCallback(STRPM::LogCallback aCallback, void* apUserData) noexcept
{
    std::scoped_lock lock(g_pluginMessagingMutex);
    m_logCallback = aCallback;
    m_logUserData = apUserData;
}

void PluginMessagingService::OnPluginMessage(const NotifyPluginMessaging& acMessage) noexcept
{
    std::scoped_lock lock(g_pluginMessagingMutex);

    STRPM::Sender sender{};
    sender.connectionID = acMessage.SenderConnectionId;
    sender.displayName = acMessage.SenderDisplayName.c_str();
    sender.isHost = acMessage.SenderIsHost;

    STRPM::Message message{};
    message.channel = acMessage.Channel.c_str();
    message.data = acMessage.PluginData.data();
    message.size = acMessage.PluginData.size();
    message.sender = sender;
    message.flags = STRPM::kMessageNone;
    message.sequence = 0;

    for (const auto& listener : m_listeners)
    {
        if (listener.Channel == acMessage.Channel)
            listener.Callback(&message, listener.UserData);
    }
}