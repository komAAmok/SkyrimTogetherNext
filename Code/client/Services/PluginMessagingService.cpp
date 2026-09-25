#include <World.h>

#include <Services/PluginMessagingService.h>
#include <Services/TransportService.h>

#include <Messages/PluginMessagingRequest.h>

#include <spdlog/spdlog.h>

#include <cstring>
#include <mutex>

namespace
{
// The registry is touched from the game thread (plugin calls) and from the
// network thread (incoming payloads), so every access is locked. Callbacks are
// collected under the lock and invoked after it is released: a plugin that
// sends from inside its own receive handler would otherwise deadlock on a
// non-recursive mutex.
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
    m_channels.clear();
    m_transportCallback = nullptr;
    m_transportUserData = nullptr;
    m_pWorld = nullptr;
}

STRPM::Result PluginMessagingService::StartTransport(STRPM::ReceiveCallback aCallback, void* apUserData) noexcept
{
    // The facade calls this with its own dispatcher and expects the callback to
    // be retained until stop(), so the payload can be handed back to it.
    std::scoped_lock lock(g_pluginMessagingMutex);
    m_transportCallback = aCallback;
    m_transportUserData = apUserData;
    return STRPM::Result::kOk;
}

STRPM::Result PluginMessagingService::StopTransport() noexcept
{
    std::scoped_lock lock(g_pluginMessagingMutex);
    m_transportCallback = nullptr;
    m_transportUserData = nullptr;
    return STRPM::Result::kOk;
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

    const auto channel = it->second;
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
        // The plugin echoes back the identity it was handed for a sender, which
        // is the framework's PlayerId.
        request.TargetPlayerId = static_cast<std::uint32_t>(aTarget.connectionID & 0xFFFFFFFFu);
        break;
    case STRPM::TargetKind::kAllPlayers:
    default:
        request.TargetKind = PluginMessagingRequest::Target::kAllPlayers;
        break;
    }

    if (aSize > 0)
    {
        const auto* pBytes = static_cast<const std::uint8_t*>(apData);
        request.PluginData.assign(pBytes, pBytes + aSize);
    }

    // The framework's own transport carries the payload: no separate socket and
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

    // Plugins address peers by echoing back the identity they were given for a
    // sender, and receive their own the same way. That identity is the
    // framework's PlayerId; the server's transport handle is never exposed.
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
    if (acMessage.Channel.empty() || acMessage.Channel.size() > STRPM::kMaxChannelLength)
        return;

    if (acMessage.PluginData.size() > STRPM::kMaxPayloadBytes)
        return;

    STRPM::Sender sender{};
    sender.connectionID = acMessage.SenderPlayerId;
    sender.displayName = acMessage.SenderDisplayName.c_str();
    sender.isHost = acMessage.SenderIsHost;

    STRPM::Message message{};
    message.channel = acMessage.Channel.c_str();
    message.data = acMessage.PluginData.data();
    message.size = acMessage.PluginData.size();
    message.sender = sender;
    message.flags = STRPM::kMessageNone;
    message.sequence = 0;

    TiltedPhoques::Vector<Delivery> deliveries;
    {
        std::scoped_lock lock(g_pluginMessagingMutex);

        // A message is offered to every registration for the channel. A plugin
        // uses either the framework entry point or the facade, and only the path
        // it chose holds a registration, so this cannot double-deliver to one.
        for (const auto& listener : m_listeners)
        {
            if (listener.Channel == acMessage.Channel)
                deliveries.push_back(Delivery{ listener.Callback, listener.UserData });
        }

        if (m_transportCallback)
            deliveries.push_back(Delivery{ m_transportCallback, m_transportUserData });
    }

    for (const auto& delivery : deliveries)
        delivery.Callback(&message, delivery.UserData);
}