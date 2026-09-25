#include <Messages/NotifyPluginMessaging.h>

#include <TiltedCore/Serialization.hpp>

#include <algorithm>

void NotifyPluginMessaging::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteString(aWriter, Channel);
    Serialization::WriteString(aWriter, SenderDisplayName);
    Serialization::WriteVarInt(aWriter, SenderPlayerId);
    Serialization::WriteBool(aWriter, SenderIsHost);

    const auto size = static_cast<std::uint32_t>(std::min<std::size_t>(PluginData.size(), kMaxPayloadBytes));
    Serialization::WriteVarInt(aWriter, size);
    if (size > 0)
        aWriter.WriteBits(PluginData.data(), size * 8);
}

void NotifyPluginMessaging::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    Channel = Serialization::ReadString(aReader);
    if (Channel.size() > kMaxChannelBytes)
        Channel.resize(kMaxChannelBytes);

    SenderDisplayName = Serialization::ReadString(aReader);
    if (SenderDisplayName.size() > kMaxDisplayNameBytes)
        SenderDisplayName.resize(kMaxDisplayNameBytes);

    SenderPlayerId = static_cast<std::uint32_t>(Serialization::ReadVarInt(aReader) & 0xFFFFFFFFu);
    SenderIsHost = Serialization::ReadBool(aReader);

    const auto size = static_cast<std::uint32_t>(Serialization::ReadVarInt(aReader));
    const auto bounded = std::min<std::uint32_t>(size, kMaxPayloadBytes);
    PluginData.resize(bounded);
    if (bounded > 0)
        aReader.ReadBits(PluginData.data(), bounded * 8);
}