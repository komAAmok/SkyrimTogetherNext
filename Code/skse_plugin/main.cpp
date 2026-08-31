// SKSE plugin entry point: boots the Skyrim Together client when the game is
// launched through the SKSE loader (e.g. skse64_loader.exe via Mod Organizer 2)
// instead of the bundled SkyrimTogether.exe launcher.
//
// skse64 loads plugins before the game's CRT startup, which is the same
// relative position the launcher bootstraps from: initializing the address
// library + engine hooks here means BeginMain runs before the game enters
// main(), exactly like the launcher flow.

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <memory>

#include <Memory.hpp> // TiltedPhoques::String

// defined in the statically linked SkyrimTogetherClient library (Code/client/main.cpp)
extern void RunTiltedInit(const std::filesystem::path& acGamePath, const TiltedPhoques::String& aExeVersion);
extern void RunTiltedApp();

namespace
{
// skse64 PluginInfo layout (see the skse64 plugin api)
struct SksePluginInfo
{
    uint32_t infoVersion;
    const char* name;
    uint32_t version;
};

constexpr uint32_t kSksePluginInfoVersion = 1;

TiltedPhoques::String QueryGameVersion()
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

extern "C" {
__declspec(dllexport) bool SKSEPlugin_Query(const void*, SksePluginInfo* apInfo)
{
    if (!apInfo)
        return false;

    apInfo->infoVersion = kSksePluginInfoVersion;
    apInfo->name = "SkyrimTogether";
    apInfo->version = 1;
    return true;
}

__declspec(dllexport) bool SKSEPlugin_Load(const void*)
{
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        return false;

    std::filesystem::path gamePath(exePath);
    gamePath.remove_filename();

    const auto version = QueryGameVersion();
    if (version.empty())
        return false;

    RunTiltedInit(gamePath, version);
    RunTiltedApp();

    return true;
}
}
