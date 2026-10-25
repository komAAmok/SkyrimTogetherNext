#pragma once

#include <Structs/GameId.h>

#include <Structs/Vector3_NetQuantize.h>
#include <Structs/Rotator2_NetQuantize.h>
#include <Structs/AnimationVariables.h>

using TiltedPhoques::Buffer;

struct Movement
{
    Movement() = default;
    ~Movement() = default;

    bool operator==(const Movement& acRhs) const noexcept;
    bool operator!=(const Movement& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    GameId CellId{};
    GameId WorldSpaceId{};
    Vector3_NetQuantize Position{};
    Rotator2_NetQuantize Rotation{};
    AnimationVariables Variables{};
    float Direction{};

    // When the owner sampled this snapshot, on the server's clock. Carried per actor rather than
    // per packet because one packet interleaves actors whose snapshots were taken at different
    // moments (the server relays each actor once, whenever that actor's owner reported it). The
    // receiver interpolates against this instead of the packet's relay time, so a slow or bursty
    // relay no longer shows up as a change of walking speed.
    //
    // This used to sit on ReferenceUpdate, which wrapped a Movement with its own action event
    // list; actions now travel in their own messages (RequestActionEvents / NotifyActionEvents),
    // so the snapshot is only the movement.
    uint64_t SampleTick{};
};
