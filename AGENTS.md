# Agent Development Guide

**Skyrim Together Next** is a fork of Skyrim Together (codename TiltedEvolution), a multiplayer mod for
Skyrim Special Edition: co-op questing, combat and exploration for 2-8 players.

- Stack: C++20 / MSVC, xmake 3.x (deps pinned in the root `xmake.lua`, `Libraries/Tilted*` and `plugins/*`
  are submodules). The UI is Angular (`Code/skyrim_ui`).
- Platforms: the client and launcher are Windows x64 only. The server also runs on Linux (amd64), so keep
  server and shared code portable.
- Game versions: **1.5.3 - 1.7.104 are all first-class**. The supported-version gate lives in
  `Code/client/main.cpp`; 1.6.x/1.7.x use the ordinary engine layouts and 1.5.x is guarded by
  `#ifdef SKYRIM_TARGET_LEGACY`. Address ID translation for 1.5.x comes from the AE-to-SE map shipped in
  `Tools/`; run `python Tools/Scripts/gen_ae_to_se_map.py` for the coverage figure rather than quoting one.
- Branches: `main` is the release branch, `prerel` for pre-releases. Open PRs against `main`.
- Logs: `<game root>\logs\tp_client.log` for the client, `logs/STServerOut.log` for the server.

## Architecture

**Client-authoritative, the server mostly relays.** Clients run the simulation. The server keeps a light world
model (players, parties, cells, which actors and objects exist, who owns them) and decides who receives what.
It never simulates Skyrim.

- The party leader effectively hosts the world. Quest progress, weather, time etc. come from the leader and
  are applied on the members' clients.
- Every synced actor or object has one owner client that runs the real AI and physics and streams its state.
  Other clients show a *remote* copy that replays it. Ownership moves when the owner leaves the cell or
  disconnects. Players are always owned by their own client.
- Clients are trusted. Validate on the server only where it matters (parties, ownership, admin commands).

**Engine integration (an SKSE plugin, unlike upstream).** `SkyrimTogetherSKSE.dll` (`Code/skse_bootstrap`)
is loaded by SKSE and picks the runtime DLL for the detected game version - `SkyrimTogetherRuntime.dll` for
1.6.x/1.7.x, `SkyrimTogetherRuntime_1_5.dll` for 1.5.x - then deploys the payload from
`Data/SkyrimTogetherRuntime/` into the game root. The client itself then loads the Address Library, checks the
game version, installs hooks, and lets other SKSE plugins keep working.

- Resolve game functions and globals through Address Library IDs, never raw addresses. On runtimes whose
  library has no entry for an id, fail closed (`FindAddressById` + a degraded path), never dereference a stub.
- Hooks are MinHook detours added with `TP_HOOK` in a static `TiltedPhoques::Initializer` at the bottom of the
  engine file, calling the original through a saved pointer. Copy `Code/client/Games/Skyrim/Actor.cpp`.
- We don't use CommonLibSSE. Engine types are our own reversed headers in `Code/client/Games/Skyrim/**`
  (`Actor`, `TESObjectREFR`, `ExtraData`, `Havok`...), with layouts pinned by `static_assert(offsetof(...))`/
  `sizeof`. A local CommonLibSSE checkout is fine as a reference; in its `Offsets*.h` the 1.6.x IDs are in the
  first `#ifdef SKYRIM_SUPPORT_AE` block.
- `GameFiles/Skyrim` holds what ships alongside the binaries: the ESPs, Papyrus scripts, meshes and behavior
  files.

**Data flow.**
1. Upstream: an engine hook or per-frame service poll notices a change. Hooks often run on engine worker
   threads, so queue the work onto the main thread through the runner service. A client service then sends a
   request through the transport service.
2. Server: a service handles the request with the sending player, updates its world model and relays a
   `Notify*` to everyone, players in range or the party, usually excluding the sender.
3. Downstream: a client service subscribed to the `Notify*` applies it to the game. While applying remote
   changes, guard against local hooks echoing them back upstream (existing services show how).

**ECS (`entt`).** Client and server each have a `World` (an `entt::registry` subclass) and an `entt::dispatcher`.

- **Services** (`Code/{client,server}/Services`): long-lived singletons in `World::ctx()` holding feature
  logic. They connect in their constructors (`aDispatcher.sink<T>().connect<&Svc::Fn>(this)`) and keep the
  connections in `entt::scoped_connection` members.
- **Components** (`Code/{client,server}/Components`): plain data. Data that belongs to an actor or object
  goes in a component on its entity. Use a service-side map keyed by form ID only if there's no other
  option. On the client, `FormIdComponent` links an entity to its form and `LocalComponent`/`RemoteComponent`
  mark the owner side.
- **Systems** (`Code/client/Systems`): near-stateless helpers over components (interpolation, animation...).
- **Events** (`Code/{client,server}/Events`): small structs passed through the dispatcher.

## Network protocol

- Transport is `Libraries/TiltedConnect` (Valve GameNetworkingSockets). All messages live in `Code/encoding`
  (`SkyrimEncoding`), shared by client and server. New messages follow the existing ones' naming, structure
  and registration.
- Reuse payload types from `Code/encoding/Structs`: `GameId` (`ModId` + `BaseId`, **never raw runtime form
  IDs**), `Vector3_NetQuantize`, `Rotator2_NetQuantize`.
- **No backward compatibility:** the client sends `BUILD_COMMIT` in `AuthenticationRequest` and the server
  rejects any mismatch (`kWrongVersion`), so reorder or change opcodes and fields freely - but see
  `docs/PITFALLS.md` before narrowing or widening a wire field on an existing opcode.
- **Every count, length and index read off the wire is untrusted.** Clamp it to the receiver's own bound
  (`Code/encoding/WireLimits.h`), consume the bytes anyway so the stream does not desync, and never throw
  out of a `DeserializeRaw` - the call chain is `noexcept`.
- Watch bandwidth. Sends are reliable by default; send high-rate, loss-tolerant data (movement) unreliably.

## Building

From the repo root (VS 2022 developer shell or PowerShell with MSVC):

```sh
xmake f -m releasedbg -y                 # configure (debug | releasedbg | release); first run fetches deps
xmake -y                                 # build everything
xmake build SkyrimTogetherClient         # one target; also SkyrimImmersiveLauncher, SkyrimTogetherServer
xmake build TPTests && xmake run TPTests # main unit tests (Catch2)
xmake project -k vsxmake                 # regenerate the VS solution; rerun after adding/removing files
```

- The solution is `vsxmake2022/<repo dir>.sln`. Most contributors build and debug from it; agents don't need it.
- **Both runtimes come out of one configuration.** `SkyrimTogetherClient` plus `SkyrimTogetherClientLegacy`
  (compiled with `SKYRIM_TARGET_LEGACY=1`) are built by the same `xmake` invocation; there is no
  per-game-version configure. The `skyrim_version` option exists for CI labelling only and changes nothing
  that is compiled - do not gate code on it.
- UI: `cd Code/skyrim_ui && pnpm install && pnpm deploy:production`. `xmake install -o distrib` gathers
  binaries.
- Main targets:
  - `SkyrimTogetherSKSE` -> `SkyrimTogetherSKSE.dll`, the SKSE entry point that deploys the runtime
  - `SkyrimTogetherClient` / `SkyrimTogetherClientLegacy` -> the two runtime DLLs
  - `SkyrimImmersiveLauncher` -> `SkyrimTogether.exe` (standalone launch path), linked against the client
  - `SkyrimTogetherServer` -> `STServer.dll`, run by `SkyrimServerRunner` -> `SkyrimTogetherServer.exe`
  - `ImmersiveElf` -> `EarlyLoad.dll` (launcher early loader), `TPProcess` (UI overlay helper process)
  - Shared static libs: `SkyrimEncoding`, `CommonLib`, `BaseLib`
  - Tests: `TPTests`, plus `Console_Tests`, `ESLoader_Tests` and `Resources_Tests` for the component libs
    in `Code/components`
- Companion plugins live in `plugins/` as submodules and are built separately (CMake + vcpkg); see
  `docs/COMPANION-PLUGINS.md`.

## Testing

- **Most gameplay features can only be verified by hand:** run a local server plus two or more game clients
  on one machine and exercise the feature from both sides. Say plainly when a change still needs an
  in-game test. Compiling is not proof that it works.
- Add unit tests only for mission-critical, Skyrim-independent code: protocol encoding, timing and
  interpolation math, pure data structures. Such code can be compiled into `TPTests`. Ask before writing
  tests for gameplay code.
- `Tools/Scripts/check_*.py` are the repository gates (docs, workflows, plugin payload, wire symmetry...).
  `python Tools/Scripts/<gate>.py` before pushing; CI runs the same files.

## Reverse engineering

- **IMPORTANT: if a task depends on engine behavior the source doesn't show (hook points, struct layouts,
  vtable slots, threading) and no IDA or Ghidra MCP tools are available, stop before implementing.**
  Say what needs verifying in the binary, warn that a source-only version may be wrong or suboptimal (guessed
  offsets, wrong hook point, missed side effects), and ask whether to go ahead.
- For changes that depend on engine behavior, the PR body must list what was verified (IDA/Ghidra evidence)
  and what remains assumed; anything not checked through an MCP counts as assumed.
- Check every supported runtime, not just the one on the dev machine: an offset that holds on 1.5.97 and one
  that holds on 1.7.104 are different numbers, and this repository asserts both at compile time. A layout
  claim without a `static_assert` behind it is a comment, not a fact.
- Map Address Library IDs to IDB addresses and back with `python Tools/Scripts/addrlib.py <decimal id | IDB address>...`
  (1.6.1170 by default, `-v 1.7.104` or `-v 1.5.97` for another version; usage at the top of the script).
- `Tools/Scripts` has other helpers for addresses, RTTI dumps and IDB matching.

## Documentation is part of the change

Every version change updates the documentation in the same commit - `CHANGELOG.md` always, the README
version table when a version is released, `docs/PITFALLS.md` for the reasoning behind a fix. The rules and
the table of what to update live in `CODE_GUIDELINES.md`; `python Tools/Scripts/check_docs.py` is the gate.

## Conventions

- **Match the surrounding style.** Hungarian-style prefixes where the file uses them: `a` arguments, `c` const,
  `p` pointers, combined as needed (`acEvent`, `apActor`). No `r` prefix for references.
- Members use `m_` (plain-data structs use bare `PascalCase` fields). Statics use `s_`, globals `g_`; hook originals are
  `Real<Name>`, constants `k<Name>`. Types and functions are `PascalCase`, and most functions are `noexcept`.
  Reversed engine types keep the game's names (`hCaster`, `slotType`).
- No exceptions and no throwing STL calls. No `auto` for primitive types.
- Use `TiltedPhoques` containers (`String`, `Vector`, `UniquePtr`...) where the surrounding code does. Log
  with `spdlog`.
- Gate debug-only UI and behavior with `IS_MASTER` / `DEBUG`. Mod compatibility shims live in
  `Code/client/ModCompat`, shipped plugin patches in `Code/plugins/patches`.
- Commits: short imperative title describing the user-visible effect ("Fix X when Y", "Stop X doing Y"),
  and a short body explaining why. Tags and commit tags are described in `CODE_GUIDELINES.md`.
