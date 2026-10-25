#include <Messages/DialogueRequest.h>
#include <WireLimits.h>

void DialogueRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, ServerId);
    Serialization::WriteString(aWriter, SoundFilename);
    SceneId.Serialize(aWriter);
    Serialization::WriteVarInt(aWriter, ScenePhase);
}

void DialogueRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    ServerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    SoundFilename = Serialization::ReadString(aReader);
    WireLimits::Clamp(SoundFilename, WireLimits::kMaxSoundPathBytes);
    SceneId.Deserialize(aReader);
    ScenePhase = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
