[中文](README.md) | [English](README_EN.md)

# Skyrim Together Next

[![Build windows](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml/badge.svg)](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml)
[![Discord](https://img.shields.io/discord/247835175860305931.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/skyrimtogether)

An open-source framework that brings multiplayer to Bethesda games. Currently supports **The Elder Scrolls V: Skyrim Special Edition**, widely known as **Skyrim Together**.

This repository extends the upstream [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution) with several practical capabilities:

## ✨ Features

- **Multiple game versions**: Full 1.6.x line (1.6.1170 / 1.6.640, etc.), new 1.7.x builds (1.7.99 / 1.7.104), and **legacy 1.5.x (including 1.5.97)** — address libraries and ID mapping tables are all bundled;
- **Seamless MO2 + SKSE launch**: Ships as an SKSE plugin; launch `skse64_loader.exe` normally through Mod Organizer 2 and the mod loads automatically, with a **self-deploying runtime** — on first launch the required files are deployed to the game root automatically, no manual copying needed;
- **Guided graphical installer (Chinese / English)**: The mod package embeds a FOMOD installer (natively supported by MO2). The first step picks Chinese or English and the remaining steps follow that choice; multiplayer files install automatically, leaving only the two genuinely optional choices (1.6.x launch validation script, standalone launcher) to you;
- **Graphical server control panel**: The Windows dedicated server ships with a minimal GUI (status / player count / logs / start-stop buttons); pass `--nogui` to fall back to pure console mode;
- **LAN multiplayer out of the box**: The networking layer is pure UDP / GameNetworkingSockets, with no Steam dependency — works over LAN, Radmin LAN, Hamachi, public VPS / Docker, or any other network topology;
- **Automatic release builds**: Pushing a tag triggers a Windows build in GitHub Actions and publishes two packages — `SkyrimTogetherNextMod-<version>` (client mod) and `SkyrimTogetherNextServer-<version>` (dedicated server).

## 🚀 Quick Start (Players)

1. Download the two zips (Mod package and Server package with matching version numbers) from this repo's [Releases](../../releases);
2. **Host**: Unzip the Server package and run `SkyrimTogetherServer.exe` (graphical UI, listens on UDP 10578 by default — remember to allow it through the firewall);
3. **All players (including the host)**: Install the Mod package via MO2 "Install from file" (the install wizard pops up), or unzip manually into the game `Data/`; launch SKSE through MO2;
4. In-game, press **F2** to bring up the multiplayer menu and connect to `<host IP>:10578`.

For illustrated LAN / Radmin instructions, see [docs/LAN-RADMIN-GUIDE.md](docs/LAN-RADMIN-GUIDE.md).

## 📦 Supported Game Versions

| Game version | SKSE | Notes |
|---|---|---|
| 1.5.3 ~ 1.5.97 (old SE) | SKSE 2.0.x | Fully supported: see note below |
| 1.6.317 ~ 1.6.1179 | SKSE 2.1.x / 2.2.x | Works out of the box (recommended) |
| 1.7.99 / 1.7.104 | New SKSE | Works out of the box (format 5 address library) |

> **1.5.x support note**: 1.5.x (including 1.5.97) is fully supported. The AE ID → 1.5.x offset translation tables for 10 versions are bundled; address mapping coverage is **99.7%** (3066 of the 3075 addresses referenced in code are resolved), and all 3699 mappings can be found in the official address library `version-1-5-97-0.bin`; the remaining 9 (near-twin functions, modules with no callers, debug views) safely degrade at runtime (empty stub / RTTI null-pointer guard / skip patch) and will not crash. The manifest and individual impact are documented in [Tools/missing_1_5_97_ids.txt](Tools/missing_1_5_97_ids.txt).
>
> **1.5.x structures**: The package bundles a **second runtime DLL** (`SkyrimTogetherRuntime_1_5.dll`). It is compiled from the same source as the one used for 1.6.x / 1.7.x, but with the `SKYRIM_TARGET_LEGACY` conditional compilation selecting the 1.5.x structure layout (typically 8 bytes smaller than 1.6.x — 1.5.x's `ExtraDataList` has no vtable; `TESObjectCELL` is missing three trailing members, a 32-byte difference). The SKSE bootstrap plugin picks one automatically based on the game version. The layout is validated member-by-member at **compile time** via `static_assert`, not "compiled against 1.6.x and hoped it matches at runtime".

## ⚠️ Known Conflicting Mods

The following mods conflict with this framework. Installing them causes crashes, UI glitches, or broken functionality, so **do not enable them at the same time**:

| Mod name | Link | Chinese alias |
| --- | --- | --- |
| Floating Subtitles | https://www.nexusmods.com/skyrimspecialedition/mods/154424 | 浮动字幕 |

> **Floating Subtitles (po3_FloatingSubtitles)**: This mod's trampoline stub jumps into an unallocated memory page and crashes the game outright when entering a room / preparing to connect. If you hit a crash on room entry, disable this mod first. If you genuinely need subtitle functionality, use an alternative implementation or wait for an upstream fix.

## 🐛 Reporting Issues

Please open an issue in the repo's "Issues" page, attaching reproducible steps, game version, SKSE version, and server logs. Detailed reports are very important for development.

## 📄 License

[![GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

This project is licensed under GPLv3 (inherited from Tilted Online / TiltedEvolution). You are free to use, modify, and redistribute it; derivative works must remain under the same license.
