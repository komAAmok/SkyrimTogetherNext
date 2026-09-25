#include <Messages/PluginMessagingRequest.h>

#include <TiltedCore/Serialization.hpp>

#include <algorithm>

void PluginMessagingRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteString(aWriter, Channel);
    aWriter.WriteBits(static_cast<std::uint8_t>(TargetKind), 8);
    Serialization::WriteVarInt(aWriter, TargetConnectionId);

    // Clamped, not asserted: a plugin that hands us an oversized payload gets a
    // truncated message rather than a corrupt stream that desynchronises every
    // later message on the connection.
    const auto size = static_cast<std::uint32_t>(std::min<std::size_t>(PluginData.size(), kMaxPayloadBytes));
    Serialization::WriteVarInt(aWriter, size);
    if (size > 0)
        aWriter.WriteBits(PluginData.data(), size * 8);
}

void PluginMessagingRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    Channel = Serialization::ReadString(aReader);
    if (Channel.size() > kMaxChannelBytes)
        Channel.resize(kMaxChannelBytes);

    std::uint64_t target = 0;
    aReader.ReadBits(target, 8);
    TargetKind = static_cast<Target>(target);

    TargetConnectionId = Serialization::ReadVarInt(aReader);

    const auto size = static_cast<std::uint32_t>(Serialization::ReadVarInt(aReader));
    // Never trust the wire length: a peer can claim more bytes than it sent.
    const auto bounded = std::min<std::uint32_t>(size, kMaxPayloadBytes);
    PluginData.resize(bounded);
    if (bounded > 0)
        aReader.ReadBits(PluginData.data(), bounded * 8);
}