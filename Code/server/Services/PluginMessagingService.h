#pragma once

#include <Events/PacketEvent.h>

#include <TiltedCore/Stl.hpp>

#include <cstdint>

struct World;
struct PluginMessagingRequest;

/**
 * @brief Routes opaque companion-plugin payloads between clients.
 *
 * The server deliberately does not understand the payload. It authenticates
 * the sender, enforces the size limits, and forwards the bytes to the
 * requested target - which is exactly what a plugin needs and nothing more.
 *
 * Sender identity is taken from the connection the request arrived on, never
 * from the payload, so a plugin cannot speak for another player.
 *
 * Rate limiting lives here rather than in each plugin: a misbehaving or
 * hostile client must not be able to make the server fan out unbounded traffic
 * to every player in the session.
 */
class PluginMessagingService
{
public:
    PluginMessagingService(World& aWorld, entt::dispatcher& aDispatcher);

protected:
    void OnPluginMessage(const PacketEvent<PluginMessagingRequest>& acMessage) noexcept;

private:
    // Per-connection token bucket. A morph snapshot burst is legitimate; a
    // sustained flood is not, and a dropped payload degrades a plugin instead
    // of degrading the session for everybody.
    static constexpr std::uint32_t kMaxPayloadBytes = 24 * 1024;
    static constexpr std::uint32_t kMaxChannelBytes = 96;
    static constexpr std::uint32_t kBurstCapacity = 256;
    static constexpr std::uint32_t kRefillPerSecond = 128;

    [[nodiscard]] bool AllowMessage(std::uint64_t aConnectionId, std::uint32_t aBytes) noexcept;

    struct Bucket
    {
        double Tokens{ static_cast<double>(kBurstCapacity) };
        std::int64_t LastRefillMs{ 0 };
    };

    World& m_world;
    entt::scoped_connection m_pluginMessageConnection;
    TiltedPhoques::Map<std::uint64_t, Bucket> m_buckets;
};