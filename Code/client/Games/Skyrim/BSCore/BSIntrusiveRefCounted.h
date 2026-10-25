#pragma once

// A game-side reference counted object whose count lives in the first word of
// the object. The count is the game's own: the game increments and decrements
// it through InterlockedIncrement/InterlockedDecrement, so anything we do to it
// has to go through the same interlocked primitives.
struct BSIntrusiveRefCounted
{
    uint32_t IncRef() const noexcept { return InterlockedIncrement(&refCount); }
    uint32_t DecRef() const noexcept { return InterlockedDecrement(&refCount); }

    mutable volatile LONG refCount;
};

static_assert(sizeof(BSIntrusiveRefCounted) == 0x4);
