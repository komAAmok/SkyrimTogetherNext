#include <TiltedOnlinePCH.h>
#include "TiltedOnlineApp.h"
#include <Misc/GameVM.h>

extern std::unique_ptr<TiltedOnlineApp> g_appInstance;

struct Main;

TP_THIS_FUNCTION(TVMUpdate, int, GameVM, float);
TP_THIS_FUNCTION(TMainLoop, short, Main);
TP_THIS_FUNCTION(TVMDestructor, uintptr_t, void);

static TVMUpdate* VMUpdate = nullptr;
static TMainLoop* MainLoop = nullptr;
static TVMDestructor* VMDestructor = nullptr;

// Counts consecutive vm ticks that reported the vm as inactive. See HookVMUpdate
// for why a long run of them is treated as a broken offset rather than an idle
// game.
static int s_inactiveFrames = 0;

int TP_MAKE_THISCALL(HookVMUpdate, GameVM, float a2)
{
    // inactive is how the game parks the vm (main menu, loading, pause). The
    // runner lives on this tick, so honouring the flag exactly is what makes the
    // client idle outside the game.
    //
    // The cost of being wrong about where the flag sits is not a bad frame, it
    // is a silent, total stop: World::Update() never runs, the task queue is
    // never drained, and every attempt to connect is queued and forgotten while
    // the overlay waits forever on "connecting". That exact failure shipped once
    // because 1.5.97 keeps inactive at 0x680 while the shared struct said 0x690.
    //
    // The flag is therefore treated as untrusted. A run of consecutive frames
    // that all report "inactive" is far past any real menu or load screen, so at
    // that point the tick is let through and the disagreement is logged instead
    // of being allowed to hang the session. A genuine idle state reports the same
    // flag on every frame anyway, so the recovery costs one extra Update() per
    // kInactiveFrameLimit frames and changes nothing else.
    static constexpr int kInactiveFrameLimit = 600;

    if (apThis->inactive == 0)
    {
        s_inactiveFrames = 0;
        g_appInstance->Update();
    }
    else if (++s_inactiveFrames >= kInactiveFrameLimit)
    {
        s_inactiveFrames = 0;

        static bool s_reported = false;
        if (!s_reported)
        {
            s_reported = true;
            spdlog::warn("SkyrimVM::inactive has read non-zero for {} consecutive vm ticks, which no menu or load "
                         "screen lasts; running the client update anyway. The offset this build assumes is {} and the "
                         "real one on this runtime differs, so the field is being read from the wrong place",
                         kInactiveFrameLimit, static_cast<int>(SkyrimVM::kInactiveOffset));
        }

        g_appInstance->Update();
    }

    return TiltedPhoques::ThisCall(VMUpdate, apThis, a2);
}

short TP_MAKE_THISCALL(HookMainLoop, Main)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    return TiltedPhoques::ThisCall(MainLoop, apThis);
}

uintptr_t TP_MAKE_THISCALL(HookVMDestructor, void)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    return TiltedPhoques::ThisCall(VMDestructor, apThis);
}

static TiltedPhoques::Initializer s_mainHooks(
    []()
    {
        POINTER_SKYRIMSE(TMainLoop, cMainLoop, 36564);
        POINTER_SKYRIMSE(TVMUpdate, cVMUpdate, 53926);
        POINTER_SKYRIMSE(TVMDestructor, cVMDestructor, 40412);

        VMUpdate = cVMUpdate.Get();
        MainLoop = cMainLoop.Get();
        VMDestructor = cVMDestructor.Get();

        TP_HOOK(&VMUpdate, HookVMUpdate);
        TP_HOOK(&MainLoop, HookMainLoop);
        TP_HOOK(&VMDestructor, HookVMDestructor);
    });

