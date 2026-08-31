target("SkyrimTogetherClientDll")
    set_basename("SkyrimTogether")
    set_kind("shared")
    set_group("Client")
    set_symbols("debug", "hidden")
    add_defines("TARGET_PREFIX=\"st\"")

    add_includedirs(
        ".",
        "../",
        "../../Libraries/")
    add_headerfiles("**.h")
    add_files("**.cpp")

    -- whole-archive so the client's self-registering systems (hooks,
    -- animation graph descriptors, rtti) are all pulled in
    add_deps("SkyrimTogetherClient")
    add_ldflags("/WHOLEARCHIVE:SkyrimTogetherClient", { force = true })

    add_deps(
        "TiltedReverse",
        "TiltedHooks",
        "TiltedUi",
        "ImGuiImpl",
        "CommonLib")

    add_syslinks(
        "user32",
        "shell32",
        "comdlg32",
        "bcrypt",
        "ole32",
        "dxgi",
        "d3d11",
        "gdi32",
        "SetupAPI",
        "Powrprof",
        "Cfgmgr32",
        "Propsys",
        "version",
        "delayimp")

    add_packages(
        "tiltedcore",
        "spdlog",
        "minhook",
        "hopscotch-map",
        "cryptopp",
        "glm",
        "cef",
        "mem")

    -- TiltedHooks.lib contains /GL objects, so this link needs /LTCG (a
    -- /FORCE:MULTIPLE restart is not compatible with it); the duplicate
    -- definitions it used to mask were fixed at the source (inline)
    add_ldflags(
        "/LTCG",
        "/IGNORE:4254,4006",
        "/INCREMENTAL:NO", { force = true })
