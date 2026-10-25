#pragma once

#include "Message.h"
#include <Structs/Movement.h>

using TiltedPhoques::String;

/**
 * @brief Movement snapshot of actors owned by the sender. Sent unreliably, so it can be lost or arrive out of order.
 */
struct ClientReferencesMoveRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kClientReferencesMoveRequest;

    ClientReferencesMoveRequest()
        : ClientMessage(Opcode)
    {
    }

    virtual ~ClientReferencesMoveRequest() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const ClientReferencesMoveRequest& acRhs) const noexcept { return Updates == acRhs.Updates && Tick == acRhs.Tick && GetOpcode() == acRhs.GetOpcode(); }

    // Synchronized clock tick at which the owner captured the snapshot
    uint64_t Tick{};
    TiltedPhoques::Map<uint32_t, Movement> Updates{};

    // A client only ever reports the entities it owns itself, so this stays far below
    // the limit. Never trust the wire length: Serialize clamps the map to this bound before it
    // writes the count, so a larger count on the wire is a malformed packet, and Deserialize
    // drops the map for one instead of clamping - clamping would leave the reader mid-stream
    // while the fields behind the map were still to be read.
    static constexpr uint64_t kMaxUpdateCount = 0x1000;
};
