[中文](README.md) | [English](README_EN.md)

# Skyrim Together Next

[![Build windows](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml/badge.svg)](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml)
[![Discord](https://img.shields.io/discord/247835175860305931.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/skyrimtogether)

> ## ⚠️ Recommended game version: **1.5.97**
> ## 📦 Recommended mod version: **>= 1.0.41**
>
> 1.5.97 is the version this framework is best validated against, and 1.0.41 fixes the
> deployment and multiplayer crash issues. If you are on another game version or an older
> mod build, please update to these before reporting a problem.

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

## 🧩 Supported Mod Packs

The following mod pack has been validated for multiplayer and can be used with this framework directly:

| Mod pack | Link | Stability |
| --- | --- | --- |
| 神话整合 (Magic Skyrim) | https://magicskyrim.net/archives/2976 | Under testing |

> **神话整合**: Verified through the basic multiplayer flow; **stability is still under testing.**
> If you hit a crash, please attach the logs described below and mention that you are using this pack.
> Mod packs not listed here are not necessarily broken, but they are untested. If a pack ships
> animation / behavior mods that collide with this framework's behavior-variable replacement,
> you will see `BehaviorVar::Patch` lines in the log.

## 🐛 Reporting Issues

Please open an issue in the repo's "Issues" page, attaching reproducible steps, game version, SKSE version, and server logs. Detailed reports are very important for development.

**Log locations (under the game root directory):**

| Log | Path | Contents |
| --- | --- | --- |
| Client log | `<game root>\logs\tp_client.log` | Core multiplayer log: connection, sync, and crash stacks |
| UI log | `<game root>\logs\cef_debug.log` | F2 overlay (CEF) errors |

> When running multiple clients the logs are named `tp_client_instance_2.log`, `tp_client_instance_3.log`, and so on;
> the server log is `logs/STServerOut.log`. For crash reports, please include the full excerpt around the crash.

## 📜 Version History

| Version | Date | Highlights |
| --- | --- | --- |
| **1.1.1** | 2026-09-25 | Third audit round: fixed a chained dereference of a cell that is null for the whole of a load in `VisitInteriorCell`, three consecutive dereferences of the player in `Actor::Create`, and an unchecked `Actor::Create` result in `DebugService`; also corrected the guard order in `Actor::Create`, which leaked the actor it had already allocated |
| **1.1.0** | 2026-09-25 | **Performance & sync**: interpolation switched to Catmull-Rom with bounded extrapolation (removes the polyline feel and the freeze-then-snap on packet loss); frame loop 16 to 8 ms, roughly doubling the update rate; movement updates drop from O(updates x entities) to linear; fixed an engine null dereference caused by spawning one remote player twice; repository-wide `GetById` dereference audit fixing 15 unguarded dereferences |
| 1.0.41 | 2026-09-23 | Self-deploy false failures fixed: content comparison replaces the timestamp test, mtime now only a pre-filter; removed a `.str_old` cleanup for a file that is never created (it polluted the error code into `error 2`); timer quantisation measured (16 ms request is really 31.25 ms) |
| 1.0.40 | 2026-09-23 | Follow `ff 25` thunks so a hook conflict names the mod that owns it |
| 1.0.39 | 2026-09-23 | Cut the per-frame cost behind slow loads and stutter |
| 1.0.38 | 2026-09-23 | Audit the project and remove code that proves dead |
| 1.0.37 | 2026-09-23 | Close the F3 / i18n leftovers and make the F3 retest conclusive |
| 1.0.36 | 2026-09-23 | Stop CEF from de-elevating the overlay window |
| 1.0.35 | 2026-09-23 | Read `SkyrimVM::virtualMachine` at 0x200 on 1.5.97, not upstream's 0x210 |
| 1.0.34 | 2026-09-22 | Fix four wrong 1.5.x id mappings and make HookAudit name the collision |
| 1.0.33 | 2026-09-22 | Fix the v1.0.32 boot crash: gate the timer-driven frame loop |
| 1.0.32 | 2026-09-22 | Drive the frame loop from a window timer; 1.5.97 actually synchronises |
| 1.0.31 | 2026-09-21 | Connection outcome dialog, persistent menu, IME input, MainLoop heartbeat |
| 1.0.30 | 2026-09-21 | The pump only parses; dispatch goes back to the game thread |
| 1.0.29 | 2026-09-21 | The connection attempt owns a pump and reports why it failed |
| 1.0.28 | 2026-09-20 | Start the connection from the thread that asked for it |
| 1.0.27 | 2026-09-20 | Make every failed connection attempt end, and let the UI know |
| 1.0.26 | 2026-09-20 | Read `SkyrimVM::inactive` at the offset 1.5.97 actually uses |
| 1.0.25 | 2026-09-20 | Let an error clear the connecting state in the overlay |
| 1.0.24 | 2026-09-20 | Connection stability: handshake deadline and a working cancel |
| 1.0.23 | 2026-09-20 | Maintenance release |
| 1.0.22 | 2026-09-19 | Conflicting mods list (Floating Subtitles) and an English README |
| 1.0.21 | 2026-09-19 | Fix the 1.5.97 New Game crash; file the v1.0.18 compatibility audit |
| 1.0.20 | 2026-09-19 | First build after syncing upstream TiltedEvolution dev |
| 1.0.19 | 2026-09-19 | Merge upstream dev (leveled NPC sync + versioned ownership); fix all merge-introduced build errors; restore 1.5.x legacy and naked-NPC self-heal; show author and project in the FOMOD |
| 1.0.18 | 2026-09-16 | F2 overlay state fixed; 1.5.97 byte patches enabled |
| 1.0.17 | 2026-09-06 | No more reading the Papyrus VM before the game has created it |
| 1.0.16 | 2026-09-06 | The VM's own vtable now says whether the Papyrus hook is on the right function |
| 1.0.15 | 2026-09-06 | The log now says whether the Papyrus registration hook ran at all |
| 1.0.14 | 2026-09-06 | A Papyrus native that was never registered no longer takes the game down |
| 1.0.13 | 2026-09-06 | A dead jump and a shared hook target now name themselves in the log |
| 1.0.12 | 2026-09-06 | The 1.5.97 launch chain works; unverified byte patches disabled |
| 1.0.11 | 2026-09-06 | Explains a missing payload instead of blaming permissions |
| 1.0.10 | 2026-09-06 | Fixes the 1.5.97 crash and the missing overlay pump |
| 1.0.9 | 2026-09-05 | The SKSE launch path works, and 1.5.97 coverage reaches 99.7% |
| 1.0.8.1 | 2026-09-04 | Crash handler now logs registers, the AV access target, a code hexdump and a stack walk |
| 1.0.8 | 2026-09-04 | Drop the comctl32 ordinal-345 import (error 182 on Win11 24H2) |
| 1.0.7 | 2026-09-03 | Recover 5 more class-A functions via call-graph disambiguation |
| 1.0.6 | 2026-09-03 | Log load-time dependency sizes when the client fails to load |
| 1.0.5 | 2026-09-03 | Zip root mirrors `Data/` so direct installs work too |
| 1.0.4 | 2026-09-03 | Detect the real game version from SKSE; ship all address libraries |
| 1.0.3 | 2026-09-03 | Detect legacy from version strings, not the structured version |
| 1.0.2 | 2026-09-02 | PlayerCharacter legacy pad anchor (0x580 vs 0x588) |
| 1.0.1 | 2026-09-02 | Complete 1.5.x address mapping to 99.2% via a capstone matcher |
| 1.0.0 | 2026-08-30 | First release |

> Each tag maps to a [Release](../../releases) carrying two packages with matching
> versions: the client mod and the dedicated server. Per-commit history is in
> `git log`; measured findings and pitfalls are in
> [docs/PITFALLS.md](docs/PITFALLS.md).

## 📄 License

[![GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

This project is licensed under GPLv3 (inherited from Tilted Online / TiltedEvolution). You are free to use, modify, and redistribute it; derivative works must remain under the same license.
