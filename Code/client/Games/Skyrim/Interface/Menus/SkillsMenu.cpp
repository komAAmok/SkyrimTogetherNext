
// The game calls it statsmenu.
#include <Interface/Menus/SkillsMenu.h>

#include <Games/GamePatch.h>

static TiltedPhoques::Initializer s_skillsMenuInit(
    []()
    {
        // https://github.com/Vermunds/SkyrimSoulsRE/blob/master/src/Menus/StatsMenuEx.cpp
        // Hoooks from souls RE
        if (auto* pProcessMessage = GamePatch::Anchor(52510, "stats menu"))
        {
            // Fix for menu not appearing
            GamePatch::Nop(pProcessMessage + 0x84E, 6, "stats menu appear");
            // Prevent setting kFreezeFrameBackground flag
            GamePatch::Nop(pProcessMessage + 0xA10, 4, "stats menu background");
            // Keep the menu updated
            GamePatch::Nop(pProcessMessage + 0x1040, 2, "stats menu update");
        }

        // Fix for controls not working
        if (auto* pControlPatch = GamePatch::Anchor(52518, "stats menu controls"))
        {
            GamePatch::Nop(pControlPatch + 0x46, 4, "stats menu controls");
            GamePatch::Nop(pControlPatch + 0x4A, 2, "stats menu controls");
        }
    });
