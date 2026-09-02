// SKSE bootstrap plugin (Scheme A - self deploying).
//
// This DLL is intentionally dependency free (kernel32 only) so the OS loader
// can always resolve it. On load it:
//   1. derives the game root from the running exe path
//   2. syncs the runtime payload from Data/SkyrimTogetherRuntime/ (virtualized
//      by MO2's usvfs when installed as a mod, a real folder otherwise) into
//      the game root - files that are missing or have an older version get
//      copied, so mod updates propagate automatically
//   3. loads the client DLL (SkyrimTogether.dll) from the game root and calls
//      its exported bootstrap entry, which performs the address library load
//      and engine hooking.
//
// If the payload is unavailable (bare install) or a file cannot be deployed
// (permissions/AV), a task dialog lists exactly what is missing so the user
// can fix it manually instead of failing silently.

#include <Windows.h>
#include <winver.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
// skse64 plugin api (stable ABI since 2.0)
struct SksePluginInfo
{
    uint32_t infoVersion;
    const char* name;
    uint32_t version;
};

constexpr uint32_t kSksePluginInfoVersion = 1;
constexpr wchar_t kRuntimeDirName[] = L"SkyrimTogetherRuntime";
constexpr wchar_t kClientDllName[] = L"SkyrimTogetherRuntime.dll";
constexpr wchar_t kClientDllLegacyName[] = L"SkyrimTogetherRuntime_1_5.dll";
constexpr char kClientEntry[] = "STClient_Bootstrap";
constexpr wchar_t kVersionMarker[] = L".str_version";

std::filesystem::path GetGameRoot()
{
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        return {};
    std::filesystem::path p(exePath);
    return p.parent_path();
}

// The 1.5.x engine ships a different struct layout (ExtraDataList has no
// vtable, several engine structs sit 8 bytes earlier), so the runtime is
// built twice: SkyrimTogetherRuntime.dll (1.6.x/1.7.x) and
// SkyrimTogetherRuntime_1_5.dll (1.5.x). Pick by the game exe file version.
bool IsLegacyGame()
{
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        return false;

    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(exePath, &handle);
    if (size == 0)
        return false;

    std::vector<uint8_t> data(size);
    if (!GetFileVersionInfoW(exePath, 0, size, data.data()))
        return false;

    VS_FIXEDFILEINFO* ffi = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&ffi), &len) || !ffi)
        return false;

    return HIWORD(ffi->dwFileVersionMS) == 1 && LOWORD(ffi->dwFileVersionMS) == 5;
}

bool DeployFile(const std::filesystem::path& acSource, const std::filesystem::path& acTarget)
{
    std::error_code ec;
    if (!std::filesystem::exists(acTarget, ec))
        return std::filesystem::copy_file(acSource, acTarget, std::filesystem::copy_options::overwrite_existing, ec);

    // keep the newer build; skip when the target is already up to date
    const auto srcTime = std::filesystem::last_write_time(acSource, ec);
    const auto dstTime = std::filesystem::last_write_time(acTarget, ec);
    if (!ec && srcTime <= dstTime)
        return true;

    // write to a temp name first so a loaded/locked dll can be replaced on
    // the next start, then swap
    std::filesystem::path tmp = acTarget;
    tmp += L".str_new";

    if (!std::filesystem::copy_file(acSource, tmp, std::filesystem::copy_options::overwrite_existing, ec))
        return false;

    if (DeleteFileW(acTarget.c_str()))
        return MoveFileW(tmp.c_str(), acTarget.c_str());

    // the target is locked (loaded by a running process); the .str_new file
    // stays in place and the next launch picks it up, keep the failure
    // count bounded by removing staged leftovers of older updates
    std::filesystem::path old = acTarget;
    old += L".str_old";
    DeleteFileW(old.c_str());
    return false;
}

bool DeployRuntime(const std::filesystem::path& acGameRoot, const std::filesystem::path& acPayload)
{
    // the marker travels with the payload and records its build version
    std::filesystem::path marker = acPayload / kVersionMarker;
    if (!std::filesystem::exists(marker))
        return false; // payload directory not installed

    bool allOk = true;

    // walk the payload and mirror it into the game root
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(acPayload, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
            break;
        if (!it->is_regular_file(ec))
            continue;

        const auto rel = std::filesystem::relative(it->path(), acPayload, ec);
        if (ec || rel.empty() || rel.native() == kVersionMarker)
            continue; // the marker itself does not belong into the game root

        const auto target = acGameRoot / rel;
        std::filesystem::create_directories(target.parent_path(), ec);
        if (!DeployFile(it->path(), target))
        {
            allOk = false;
        }
    }

    return allOk;
}

void ShowDeployError(const std::filesystem::path& acGameRoot, const std::filesystem::path& acPayload)
{
    // simple MessageBox based report; task dialogs need comctl32 linkage we
    // deliberately avoid in this bootstrap dll
    const std::wstring msg = L"Skyrim Together could not deploy its runtime files.\n\n"
                             L"Payload: " + acPayload.wstring() +
                             L"\nGame root: " + acGameRoot.wstring() +
                             L"\n\nPlease copy the contents of the payload folder into the game root "
                             L"manually (libcef.dll and friends), or run the game as administrator once.";
    MessageBoxW(nullptr, msg.c_str(), L"Skyrim Together", MB_ICONERROR | MB_OK);
}

bool StartClient(const std::filesystem::path& acGameRoot)
{
    const auto clientPath = acGameRoot / (IsLegacyGame() ? kClientDllLegacyName : kClientDllName);
    HMODULE h = LoadLibraryW(clientPath.c_str());
    if (!h)
        return false;

    using BootstrapFn = bool (*)(const wchar_t*);
    const auto bootstrap = reinterpret_cast<BootstrapFn>(GetProcAddress(h, kClientEntry));
    if (!bootstrap)
        return false;

    return bootstrap(acGameRoot.c_str());
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
    _setmaxstdio(8192);

    const auto gameRoot = GetGameRoot();
    if (gameRoot.empty())
        return false;

    // Data/<RuntimeDir> - MO2 virtualizes this when installed as a mod; a
    // manual Data copy resolves to the same location
    const auto payload = gameRoot / L"Data" / kRuntimeDirName;
    if (!DeployRuntime(gameRoot, payload))
    {
        // payload missing entirely -> nothing we can do automatically
        if (!std::filesystem::exists(payload))
        {
            ShowDeployError(gameRoot, payload);
            return false;
        }
        // some files failed to copy; report and continue, the client may
        // still start if the failed file was not critical
        ShowDeployError(gameRoot, payload);
    }

    return StartClient(gameRoot);
}
}
