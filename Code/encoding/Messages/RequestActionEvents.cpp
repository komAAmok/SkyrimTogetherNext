#include <Messages/RequestActionEvents.h>
#include <TiltedCore/Serialization.hpp>
#include <algorithm>

void RequestActionEvents::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    // The reader drops the whole message for an outer count past kMaxActorCount and an
    // actor's list past kMaxActionCount, so writing more than that would cost the batch
    // every time it happened. Both are clamped here the way every other count in this
    // framework is, which keeps what is written inside what the reader accepts.
    const auto count = std::min<size_t>(Actions.size(), kMaxActorCount);
    Serialization::WriteVarInt(aWriter, count);

    size_t written = 0;
    for (const auto& [serverId, actions] : Actions)
    {
        if (written++ >= count)
            break;

        Serialization::WriteVarInt(aWriter, serverId);

        const auto actionCount = std::min<size_t>(actions.size(), kMaxActionCount);
        Serialization::WriteVarInt(aWriter, actionCount);

        for (size_t i = 0; i < actionCount; ++i)
        {
            actions[i].GenerateDifferential(ActionEvent{}, aWriter);
        }
    }
}

void RequestActionEvents::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    // Neither count is trustworthy. This message used to resize both the outer map and an
    // actor's list to whatever the packet claimed, so a short packet bought an allocation of
    // millions of ActionEvents before the first one was parsed.
    //
    // An over-long outer count cannot come from a well behaved peer either way, so the message
    // is dropped. An over-long list for one actor is dropped on its own: the list is the last
    // thing that actor's entry describes, so the entries behind it still start at the offset
    // the sender wrote them to, and only that actor's actions are lost.
    //
    // The map is cleared first because it is not reset on entry, and the operator[] below would
    // otherwise keep entries of a previous parse.
    const auto count = Serialization::ReadVarInt(aReader);
    if (count > kMaxActorCount)
    {
        Actions.clear();
        return;
    }

    for (auto i = 0u; i < count; ++i)
    {
        const uint32_t cServerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        auto& actions = Actions[cServerId];

        const auto actionCount = Serialization::ReadVarInt(aReader);
        if (actionCount > kMaxActionCount)
        {
            actions.clear();
            continue;
        }

        actions.resize(actionCount);

        for (auto& action : actions)
        {
            action.ApplyDifferential(aReader);
        }
    }
}
