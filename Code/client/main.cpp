
#include <TiltedOnlineApp.h>
#include <TiltedOnlinePCH.h>

#include <Commctrl.h>
#include <Windows.h>

#include <base/dialogues/win/TaskDialog.h>

std::unique_ptr<TiltedOnlineApp> g_appInstance{nullptr};

extern HICON g_SharedWindowIcon;

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
    if (!VersionDb::Get().Load(acGamePath, aExeVersion))
    {
        ShowAddressLibraryError(acGamePath.c_str(), aExeVersion);
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
