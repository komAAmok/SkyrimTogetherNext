#pragma once

#include "Message.h"
#include <Structs/ActionEvent.h>

/**
 * @brief Animation actions relayed as soon as the owner's RequestActionEvents arrives. Sent reliably.
 */
struct NotifyActionEvents final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyActionEvents;

    NotifyActionEvents()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyActionEvents& acRhs) const noexcept { return Actions == acRhs.Actions && GetOpcode() == acRhs.GetOpcode(); }

    // Server id -> actions in the order they were performed
    TiltedPhoques::Map<uint32_t, Vector<ActionEvent>> Actions{};

    // The server relays what it received, so the sender side bounds hold here too. Never trust
    // the wire length: Serialize clamps the map to these bounds before it writes the counts, so a
    // larger count on the wire is a malformed packet, and Deserialize drops the map for one
    // instead of clamping - clamping would leave the reader mid-stream while the actions behind
    // it were still to be read.
    static constexpr uint64_t kMaxActorCount = 0x400;
    static constexpr uint64_t kMaxActionCount = 0x400;
};
