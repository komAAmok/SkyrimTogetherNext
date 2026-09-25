#include <World.h>

#include <Services/PluginMessagingService.h>

#include <Messages/PluginMessagingRequest.h>
#include <Messages/NotifyPluginMessaging.h>

#include <GameServer.h>
#include <Game/Player.h>
#include <Game/PlayerManager.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

PluginMessagingService::PluginMessagingService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_pluginMessageConnection = aDispatcher.sink<PacketEvent<PluginMessagingRequest>>().connect<&PluginMessagingService::OnPluginMessage>(this);
}

bool PluginMessagingService::AllowMessage(std::uint64_t aConnectionId, std::uint32_t aBytes) noexcept
{
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

    auto& bucket = m_buckets[aConnectionId];
    if (bucket.LastRefillMs == 0)
        bucket.LastRefillMs = nowMs;

    const auto elapsedMs = nowMs - bucket.LastRefillMs;
    if (elapsedMs > 0)
    {
        bucket.Tokens = std::min(static_cast<double>(kBurstCapacity),
                                 bucket.Tokens + (static_cast<double>(elapsedMs) / 1000.0) * kRefillPerSecond);
        bucket.LastRefillMs = nowMs;
    }

    // Charged per byte so one large snapshot costs more than one small update.
    const auto cost = 1.0 + (static_cast<double>(aBytes) / 1024.0);
    if (bucket.Tokens < cost)
        return false;

    bucket.Tokens -= cost;
    return true;
}

void PluginMessagingService::OnPluginMessage(const PacketEvent<PluginMessagingRequest>& acMessage) noexcept
{
    const auto& request = acMessage.Packet;

    if (request.Channel.empty() || request.Channel.size() > kMaxChannelBytes)
        return;

    if (request.PluginData.size() > kMaxPayloadBytes)
        return;

    auto* pSender = acMessage.pPlayer;
    if (!pSender)
        return;

    // The connection id is what plugins address peers by, and it is resolved
    // from the authenticated player rather than from the payload.
    const auto senderConnectionId = static_cast<std::uint64_t>(pSender->GetConnectionId());
    if (!AllowMessage(senderConnectionId, static_cast<std::uint32_t>(request.PluginData.size())))
    {
        spdlog::warn("PluginMessaging: rate limited connection {} on channel {}", senderConnectionId, request.Channel.c_str());
        return;
    }

    NotifyPluginMessaging notify{};
    notify.Channel = request.Channel;
    notify.PluginData = request.PluginData;
    notify.SenderConnectionId = senderConnectionId;
    notify.SenderDisplayName = pSender->GetUsername();
    // There is no designated host in a dedicated-server session; the flag stays
    // false until the framework has a notion of one, rather than guessing.
    notify.SenderIsHost = false;

    switch (request.TargetKind)
    {
    case PluginMessagingRequest::Target::kAllPlayers:
        GameServer::Get()->SendToPlayers(notify, pSender);
        break;

    case PluginMessagingRequest::Target::kPlayer:
        // An unknown id is a normal race against a disconnect, not an error.
        if (request.TargetConnectionId != 0)
            GameServer::Get()->Send(static_cast<ConnectionId_t>(request.TargetConnectionId), notify);
        break;

    case PluginMessagingRequest::Target::kServer:
    case PluginMessagingRequest::Target::kHost:
        // No server-side plugin host exists yet, so the payload is accepted and
        // dropped rather than silently misrouted.
        break;

    default:
        break;
    }
}