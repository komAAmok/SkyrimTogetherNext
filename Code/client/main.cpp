
#include <TiltedOnlineApp.h>
#include <TiltedOnlinePCH.h>

#include <Commctrl.h>
#include <Windows.h>

#include <base/dialogues/win/TaskDialog.h>

#include <GameRoot.h>

std::unique_ptr<TiltedOnlineApp> g_appInstance{nullptr};

extern HICON g_SharedWindowIcon;

namespace
{
std::filesystem::path s_gameRoot;
} // namespace

const std::filesystem::path& GetGameRoot()
{
    return s_gameRoot;
}

static void ShowAddressLibraryError(const wchar_t* apGamePath, const String& acExeVersion)
{
    const bool isLegacyGame = strncmp(acExeVersion.c_str(), "1.5.", 4) == 0;

    auto errorDetail = fmt::format(L"Looking for it here: {}\\Data\\SKSE\\Plugins", apGamePath);

    const wchar_t* mainText = L"Make sure to use \"All in one (1.6.X)\"";
    if (isLegacyGame)
        mainText = L"Game version 1.5.x requires version-<version>.bin (from \"All in one (Special Edition)\") "
                   L"and the AE-to-SE id mapping file versionlib-ae-to-se-<version>.map";

    Base::TaskDialog dia(g_SharedWindowIcon, L"Error", L"Failed to load Skyrim Address Library", mainText, errorDetail.c_str());

    dia.AppendButton(0xBEED, L"Visit troubleshooting page on wiki.tiltedphoques.com");
    dia.AppendButton(0xBEEF, L"Visit Address Library modpage on nexusmods.com");
    const int result = dia.Show();
    if (result == 0xBEEF)
    {
        ShellExecuteW(nullptr, L"open", LR"(https://www.nexusmods.com/skyrimspecialedition/mods/32444?tab=files)", nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (result == 0xBEED)
    {
        ShellExecuteW(nullptr, L"open", LR"(https://wiki.tiltedphoques.com/tilted-online/guides/troubleshooting/address-library-error)", nullptr, nullptr, SW_SHOWNORMAL);
    }

    exit(4);
}

void RunTiltedInit(const std::filesystem::path& acGamePath, const String& aExeVersion)
{
    s_gameRoot = acGamePath;

    if (!VersionDb::Get().Load(acGamePath, aExeVersion))
    {
        ShowAddressLibraryError(acGamePath.c_str(), aExeVersion);
    }

    if (VersionDb::Get().IsLegacyFormat())
    {
        // legacy game versions (1.5.x) run with a partial id mapping; some
        // sync features (item data, weather, subtitles, quests) degrade to
        // no-ops instead of crashing. tell the player up front.
        MessageBoxW(nullptr,
                    L"Skyrim Together Next: legacy game version detected (1.5.x).\n\n"
                    L"Support for this version is experimental. Some sync features are unavailable and "
                    L"multiplayer desyncs are possible. For the full experience, use game version 1.6.x or newer.",
                    L"Skyrim Together Next", MB_ICONWARNING | MB_OK);
    }

    // VersionDb::Get().DumpToTextFile(R"(S:\Work\Tilted\fallout\_addresslib.txt)");

    g_appInstance = std::make_unique<TiltedOnlineApp>();

    TiltedOnlineApp::InstallHooks2();
    TP_HOOK_COMMIT;
}

void RunTiltedApp()
{
    g_appInstance->BeginMain();
}
