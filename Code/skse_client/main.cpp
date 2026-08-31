// The client runtime DLL loaded by the SKSE bootstrap plugin
// (SkyrimTogetherSKSE.dll) after it deployed this file and the rest of the
// runtime payload into the game root. STClient_Bootstrap performs the same
// boot the SkyrimTogether.exe launcher does: address library load, engine
// hooks, app start.

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

// defined in the statically linked SkyrimTogetherClient library (Code/client/main.cpp)
extern void RunTiltedInit(const std::filesystem::path& acGamePath, const TiltedPhoques::String& aExeVersion);
extern void RunTiltedApp();

// TiltedPhoques::String is declared in Memory.hpp from the client project
#include <Memory.hpp>

namespace
{
std::string QueryGameVersion()
{
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        return {};

    if (DWORD infoSize = GetFileVersionInfoSizeW(exePath, nullptr))
    {
        auto buffer = std::make_unique<uint8_t[]>(infoSize);
        if (GetFileVersionInfoW(exePath, 0, infoSize, buffer.get()))
        {
            char* version = nullptr;
            DWORD len = 0;
            if (VerQueryValueA(buffer.get(), "\\StringFileInfo\\040904B0\\ProductVersion",
                               reinterpret_cast<void**>(&version), reinterpret_cast<PUINT>(&len)) &&
                version && *version)
            {
                return version;
            }

            if (VerQueryValueA(buffer.get(), "\\StringFileInfo\\040904B0\\FileVersion",
                               reinterpret_cast<void**>(&version), reinterpret_cast<PUINT>(&len)) &&
                version && *version)
            {
                return version;
            }
        }
    }

    return {};
}
} // namespace

extern "C" __declspec(dllexport) bool STClient_Bootstrap(const wchar_t* acpGameRoot)
{
    if (!acpGameRoot)
        return false;

    // the launcher raises the stdio handle limit before booting the client
    // (cef and the game open a lot of handles); keep parity here
    _setmaxstdio(8192);

    const std::filesystem::path gamePath(acpGameRoot);

    const auto version = QueryGameVersion();
    if (version.empty())
        return false;

    RunTiltedInit(gamePath, version.c_str());
    RunTiltedApp();

    return true;
}
