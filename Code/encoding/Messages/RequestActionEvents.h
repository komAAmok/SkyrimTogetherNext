#pragma once

#include "Message.h"
#include <Structs/ActionEvent.h>

/**
 * @brief Animation actions performed by actors owned by the sender, sent reliably along with each movement snapshot.
 */
struct RequestActionEvents final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestActionEvents;

    RequestActionEvents()
        : ClientMessage(Opcode)
    {
    }

    virtual ~RequestActionEvents() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestActionEvents& acRhs) const noexcept { return Actions == acRhs.Actions && GetOpcode() == acRhs.GetOpcode(); }

    // Server id -> actions in the order they were performed
    TiltedPhoques::Map<uint32_t, Vector<ActionEvent>> Actions{};

    // An actor only ever reports the handful of actions it played since its previous snapshot, so
    // this stays far below the bound. Never trust the wire length: Serialize clamps the map to
    // this bound before it writes the count, so a larger count on the wire is a malformed packet,
    // and Deserialize drops the map for one instead of clamping - clamping would leave the reader
    // mid-stream while the actions behind it were still to be read.
    static constexpr uint64_t kMaxActorCount = 0x400;
    // Same reasoning per actor, and the same bound ReferenceUpdate uses for one actor's list.
    static constexpr uint64_t kMaxActionCount = 0x400;
};
