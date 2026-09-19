#pragma once

#include <Misc/BSScript.h>

struct SkyrimVM
{
    virtual ~SkyrimVM();

    static SkyrimVM* Get();

    // The vm as the game itself handed it to one of our papyrus hooks. That is
    // the only source that holds on both runtimes: the field below sits where
    // 1.6.x puts it, and on 1.5.97 that offset lands on unrelated data, so a
    // read there returns a non-null pointer to something that is not a vm.
    // Nothing is remembered until a hook runs, so callers still have to cope
    // with getting nothing back.
    static void SetVirtualMachine(BSScript::IVirtualMachine* apVirtualMachine) noexcept;
    static BSScript::IVirtualMachine* GetVirtualMachine() noexcept;

    // upstream corrected this from 0x200 to 0x210 (virtualMachine really sits
    // at 0x210); the fork only had the older value because it had not taken
    // that fix yet.
    uint8_t pad8[0x210 - 0x8];
    BSScript::IVirtualMachine* virtualMachine;
    uint8_t pad218[0x690 - 0x218];
    int32_t inactive;
};

static_assert(offsetof(SkyrimVM, virtualMachine) == 0x210);
static_assert(offsetof(SkyrimVM, inactive) == 0x690);

using GameVM = SkyrimVM;
