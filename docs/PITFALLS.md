# 踩坑记录与已知问题清单（提交前必读）

> 本文件是 SkyrimTogetherNext（TiltedEvolution fork）在 1.5.97 / 1.6.x / 1.7.x 联机修复过程中，
> **所有踩过的坑 + 已修复/未修复 bug** 的留档。目的只有一个：**后续每次修改、每次提交前，
> 先对照 §0 核对清单和正文，不要再重复踩坑。**
>
> 更新约定：每次修复一个新 bug 或踩一个新坑，**当场**追加到本文件；正文以"现象 → 根因 → 修复 →
> 教训"四段式记录，并带上 commit hash 作为可追溯锚点。

---

## 0. 提交前核对清单（每次 commit 前逐条过）

改动涉及哪类，就过哪几项；全绿再提交。

- [ ] **没动子模块** `Libraries/{TiltedUI,TiltedHooks,TiltedReverse,TiltedConnect}`。改了 = 白改
  （CI 前 `git submodule update --init --force --recursive` 会丢弃本地改动）。overlay/input 修复必须放 `Code/`。
- [ ] **没在本地编译**。Windows/MSVC 目标只能在 GitHub CI（`windows.yml`，push `main` 触发）验证；
  本机 xmake 只读配置。`build/`、`.xmake/` 是本地产物，可删、勿提交。
- [ ] **改动若碰了结构体偏移** → 先判"锚点式 pad"还是"相对式 pad"（§2），锚点式**禁止用「上游值−8」**；
  改完更新两分支的 `static_assert`，让下次漂移在编译期炸出来，而不是静默读邻居。
- [ ] **若同步过上游** → 跑了"新增语句行存活率"扫描 + `git diff 5a948fcd HEAD -- Code/client/Games/Skyrim/Forms/`
  找"fork 有 HEAD 无"成员（§3）。
- [ ] **若新增/改动消息类** → 核对 `T::Opcode` 全仓无重复（`ServerMessageFactory.cpp` 的
  `s_serverMessageExtractor[T::Opcode]` 是注册入口，visitor 顺序无关）。
- [ ] **若改动 UI↔C++ 桥接** → 新 JS→C++ 调用必须在 `ProcessHandler.cpp::OnContextCreated` 注册；
  C++→JS 带参数必须 `ExecuteAsync(name, pArgs)` 传 `CefListValue`（§6）。
- [ ] **若给帧循环加/换驱动源** → 复刻原驱动源的**门控条件**（`SkyrimVM::inactive` / 玩家存在性），
  否则启动期会提前跑整套 `Update()`（§5，v1.0.32 启动闪退就是这么来的）。
- [ ] **若改了 `xmake.lua` 的 `set_xmakever()`** → 与 `windows.yml` 的 `xmake-version:` 同步。
- [ ] **若新增 `GameFiles/` 内容** → `GameFiles/Skyrim/fomod/ModuleConfig.xml` 的
  `requiredInstallFiles` 跟进，否则漏装且 `IsDefaultModlist` 误报"非原版安装"。
- [ ] **若改了 TS/SCSS** → 独立 tsc 验证（比对改动前后错误数）；本项目 `rem()` 是两参数
  `rem($base,$pixels)`，别当 CSS 的单参数 `rem()` 用（§6）。
- [ ] **版本握手**：客户端/服务端必须**同一 commit** 构建（握手比对 `BUILD_COMMIT`），新旧不能混用。
- [ ] **`.workbuddy/` 不提交**（已在 `.gitignore`）；commit 前 `git status` 确认干净。

---

## 1. 硬约束（绝对不能违反）

- **禁止本地编译**（Windows/MSVC 目标）。验证一律走 GitHub CI（`windows.yml`，`windows-2022`/x64，
  push `main` 触发）。报错 → 静态分析 + 看 Actions。
- **禁止改子模块**（见 §0 第一条）。
- `gh` CLI 本机不可用；GitHub REST 匿名常 403 限流。绕行：
  - CI 状态看徽章 `.../actions/workflows/windows.yml/badge.svg?branch=main&t=$(date +%s)`
    （失败约 4 分钟显现）。
  - `github.com` 走不通时 `api.github.com` 往往通：`api.github.com/repos/<o>/<r>/actions/runs`、
    `/releases`。
  - 有凭据时 `TOKEN=$(sed -n 's#https://[^:]*:\([^@]*\)@github.com#\1#p' ~/.git-credentials)`
    加 `Authorization: Bearer $TOKEN` 查 Actions API。
- `IS_MASTER` 由 xmake `before_build` 从 **git 分支名**推导，CI 从 `main` 构建 → 发布版。
  release 里 `#if (!IS_MASTER)` 全被裁掉（F6/F7/F8 快捷连接、debug 菜单内容都不存在）。

## 2. 结构体偏移（最容易反复咬人）

**没有通用公式。先判"锚点式"还是"相对式"。**

- 相对式 pad（`pad_ptrs2[8]` vs `[9]`）：自动跟随，可用「上游值 −8」。
- **锚点式 pad（`pad1[0x580 - sizeof(Actor)]`）：锚点一个人决定整条尾巴，必须独立定值。**
  用"上游值 −8"必然重复计算 → 字段整体漂一个 qword（读成邻居）。
  → 这就是 2026-09-19 新游戏闪退的根因（`c0abe640` 修）。

`PlayerCharacter.h` legacy 真值（**照抄，别重算**）：objectives `0x580` / pSkills `0x9B0` /
locationForm `0xAC8` / baseTints `0xB10` / overlayTints `0xB28` / sizeof `0xBE0`；
AE 每项 +`0x10`。上游 `6f963497` 抬过 `pad1`，**同步时不要把 legacy 锚点跟着改**。

`SkyrimVM::inactive`（`GameVM.h`）：legacy **0x680** / AE **0x690**（差 0x10）。
`SkyrimVM::virtualMachine`（同文件）：legacy **0x200** / AE **0x210**（同样差 0x10）。

> 这个坑到目前为止咬了**三次**：① `PlayerCharacter::pad1`（`c0abe640`）；
> ② `SkyrimVM::inactive`（`b0082844`）；③ `SkyrimVM::virtualMachine`（§12.3）。
> 三次都是"合并时把上游值当成绝对值吹进 legacy 分支"。
> 上游的"修正"是**对 AE 而言**的修正——合并时先问一句
> “这是绝对值还是相对增量”。

查证手段（照抄，别重算）：
1. 逐字节哈希：`[ "$(git rev-parse v1.0.18:$f)" = "$(git rev-parse HEAD:$f)" ]`。
2. 提取 legacy 分支比对（**遇 `#else` 必须清空已收集 body**，否则取到非 legacy 侧）。
3. `static_assert` 集合对账，一次揪出所有被动过值的头文件。
4. 或 `git log -S"<pad 字面量>" --oneline -- <file>` → `git show <sha>:<file>` 照抄断言表。
5. legacy 分支还要核对**是否缺上游新增成员**（如 `TESObjectCELL::cellFlags[5]` 被拆成
   三个字段 + `IsAttached()`）⇒ 只有 1.5.x 构建失败。

判据：读到 `0xFF7FFFFF`（`-FLT_MAX`，落在 `0xFF000000..0xFF7FFFFF` 未映射哨兵带）
≈ "读偏了一个字段"，不是野指针。

## 3. 上游同步 / 合并（每次 merge 后的高危覆盖点）

- `Code/client/main.cpp`：上游硬编码 `kSupportedGameVersions = 1.7.104.0` 且不匹配就 `exit(4)`，
  fork 已改为 `1.5./1.6./1.7.` 前缀判断，合并会把它冲掉。
- `TiltedOnlineApp.cpp`：`LoadScriptExtender()` **全树只能一处调用**（两处 → papyrus native 注册异常）。
- **游戏表单头文件里 fork 新增的成员会被同名文件静默覆盖**（不在冲突列表里）：
  吃过 `TESObjectARMO::IsBodyPiece()`、`TESNPC::outfits[2]`、`VersionDb.h` 整文件。
  必做：`git diff 5a948fcd HEAD -- Code/client/Games/Skyrim/Forms/` 找"fork 有 HEAD 无"成员。
- **消息协议**：注册靠消息类静态成员 `T::Opcode`；真风险是两个类声明同一 Opcode。
- `xmake.lua` 的 `set_xmakever()` 必须与 `windows.yml` 的 `xmake-version:` 同步。
- 上游新增 `GameFiles/` 内容 → `GameFiles/Skyrim/fomod/ModuleConfig.xml` 的 `requiredInstallFiles` 跟进。
- **自动合并会静默吃掉声明和调用点** ⇒ 冲突解决完不能只 grep 函数名；用"新增语句行存活率"
  比对 fork/上游各自相对分叉点 `5a99a0b6` 的新增行是否还在。本次合并后最值钱的扫描是
  **"局部变量完整性"**（抓半截 hunk 拼接）。
- `.gitmodules` 幽灵 submodule：删掉残留 `[submodule]` 条目（`66d61839`），否则拉远程报错。

## 4. launcher vs SKSE 差异

1. 页面保护：SKSE 下 `.text` 是 RX，直写会 AV → 需临时 RWX 窗口（`GamePatch.h`）。
2. 未映射 ID → `VersionDbPtr` 返回共享 7 字节空桩；把补丁点当锚点会破坏它
   ⇒ 锚点必须用不降级的 `FindAddressById`（`GamePatch::Anchor`）。
3. 运行时 DLL 与镜像距离 ~20GB ⇒ 写 rel32 静默截断，症状 "jumped to code that is not there"，
   调用方显示成正常游戏函数（P0，`00ec7306`）。修法：先跳镜像附近桩
   （`ff 25 00000000` + 8 字节绝对地址），桩用 `RipAllocateN` 分配。`TP_HOOK`/MinHook 不受影响。

## 5. 帧循环与联机传输（连接挂死 / 不同步 / 启动闪退都在这）

**调用链唯一**：`SkyrimVM64.cpp::HookVMUpdate`（id 53926 → 1.5.97 RVA `0x921F10`）
→ `TiltedOnlineApp::Update()` → `World::Update()` → `trigger(PreUpdateEvent)` +
`RunnerService::OnUpdate` 抽干队列 + `trigger(UpdateEvent)`。
**`Client::Update()`（uv_run + RunCallbacks + 收包）全仓唯一调用点 = `TransportService.cpp:308 HandleUpdate`**
⇒ 网络推进完全挂在游戏 tick 上。

**诊断公式**（连接挂死）：
> connect 已打出 `connecting to ... giving up after 10s`，但接下来 15 秒零日志，
> **连 `Handshake ... timed out after 10s, aborting` 都没有** ⇒ `HandleUpdate` 一次没跑
> ⇒ tick 没驱动 Update。再叠加"按 Disconnect 也没有 `Disconnected from server (4): aborted`"
> ⇒ uv_loop 从没被泵过（否则 `uv_cancel` 回调必出声）。主线程还活着 ⇒ 不是泵里死循环。

**关键教训（按时间顺序，每一条都是坑）**：

1. **握手无超时 / 取消被 UI 阻断 / 重复连接叠加**（`66851aaf`）：`TransportService` 只在
   `Client::OnDisconnected`（Steam 状态回调）才知道失败；DNS 无响应或服务端不回认证时无任何
   计时器兜底；取消按钮 `[disabled]="connecting"` 且 `cancel()` 不调 `disconnect()`；
   `ProcessConnectMessage` 不先 `Close()` 就 `Connect()`。修法 = `ArmConnectionWatchdog` 10s 看门狗 +
   取消真正中止 + 先 Close 再 Connect。
2. **去重标志别合用一个 `m_attemptOutcomeReported`**（`83f3f89e` 引入，`6acd6ef8` 修正）：
   失败路径"先报错误、后 Close() 同步回调 OnDisconnected"，共用标志会被先到的置位 →
   `DisconnectedEvent` 仍不发。必须按通道拆 `m_attemptErrorReported` / `m_attemptDisconnectReported`。
3. **`ExecuteAsync("disconnect")` 不传参 = JS 收到 `undefined`**：`OverlayService::OnDisconnectedEvent`
   一直是 `ExecuteAsync("disconnect")` 无参，JS `onDisconnect(isError)` 靠 `isError` 判断自动重连 →
   **自动重连分支自 web UI 诞生起就是死代码**（上游 `dev` 同款缺陷，fork 独修）。
   修法：`DisconnectedEvent` 加 `bool IsError`，`pArgs->SetBool(0, ...)` 转发。
4. **不碰游戏内存的动作别走 `RunnerService::Queue`**（v1.0.28）：`Close()`/`Connect()`/
   `ArmConnectionWatchdog()` 已在 CEF 消息线程直接同步调用。
5. **传输泵搬离帧循环**（`5efab47f`/`d9f01e7e`）：handshake 期间专用线程 + `std::recursive_mutex`，
   pump **只解析不分发**（`OnConsume` 比对线程 id，非游戏线程则入 `m_pendingMessages`），
   再 `m_world.GetRunner().Queue` 交还游戏线程。两条不变量别破坏：
   - pump 只在帧循环缺席时跑（`HandleUpdate` 先盖 `m_lastGameThreadTickMs` 再判所有权）；
   - `FlushPendingMessages` 先 swap 出来再分发，避免 ABBA 死锁。
6. **1.5.97 两个帧钩子一次都没被调用**（v1.0.32，`a9ad774f`）：`HookVMUpdate`(0x921F10) 和
   `HookMainLoop`(0x5B2FF0) 心跳整场为 0，但钩子"0 did not land" ⇒ 这两个地址库条目**不在执行路径上**
   （map 是从历史符号 join 来的，疑似当时就贴错）。修法 = 帧循环改由**窗口定时器**驱动
   （`InputService::WndProc` 首条消息 `SetTimer(16ms)`，`WM_TIMER` 里 `World::Get().Update()`，
   `return 1` 吞掉消息）。`World::Update()` 加 8ms 自节流 + `m_hasTicked` 去重。
7. **新驱动源无门控 → 启动闪退**（`83bb37be`，v1.0.32 的修复引入）：定时器在**首条窗口消息**
   （启动期）就 armed，第一次 tick 把整套 `Update()` 在玩家还不存在时跑起来 →
   `DiscoveryService::VisitForms()` 第 251 行 `visitor(PlayerCharacter::Get())` 未判空，
   `[NULL+0x14]`（`TESForm::formID` 在偏移 0x14）read fault。AE 天然安全是因为 `HookVMUpdate`
   只在 `SkyrimVM::inactive==0` 时跑。修法 = 给定时器补等价门控
   `if (s_pOverlay && s_pOverlay->GetInGame()) World::Get().Update()`，并给第 251 行补判空。
   **教训：给帧循环加新驱动源时，必须复刻原驱动源的门控条件。**
8. **跨线程 libuv**：`ProcessConnectMessage` 在 CEF 消息线程调 `Client::Connect()`，
   而 uv loop 归游戏线程所有。字面 IP 走 `ConnectByIp()` 完全绕开 `uv_getaddrinfo`（`IsLiteralIPV4()`）。
9. **`WndProc` 转发语义**：`WindowsHook` 回调返回 0 = 转发给游戏，非 0 = 吞掉。
   `WM_TIMER` 要 `return 1`，其他分支照旧 `return 0`。

## 6. UI ↔ C++ 桥接（编译期零报错，全靠记忆）

- **新增 JS→C++ 调用必须在 `ProcessHandler.cpp::OnContextCreated` 注册**，否则
  `skyrimtogether.<fn>()` 是 `undefined`。`OverlayV8Handler::Dispatch` 把 V8 函数名原样当
  ui-event 名转发 ⇒ 名字必须逐字一致。
- **C++→JS 带参数时 `ExecuteAsync(name)` 不传 `CefListValue` = 参数全是 `undefined`。**
  加事件参数要同时改：事件结构体 + `pArgs->SetXxx` + `ExecuteAsync(...,pArgs)` + JS 形参 +
  mock + typings。
- **多通道标志位别合用一个 `m_attemptXxxReported`**（同 §5.2）。
- 日志证据：`OverlayClient.cpp:36-39` 的 4 行 ui_event info 在分发**之前**打印 →
  它们出现而目标动作首行不出现 ⇒ 事件到了、lambda 没执行。
- **TS 改动验证**：`skyrim_ui` 无本地 node_modules，用 /tmp 独立 tsc + 手写 stub `.d.ts`；
  `tsconfig.base.json` 未开 strict，比对"改动前后错误数是否相同"才算有效验证。
- **SCSS `rem()` 是两参数**（`rem($base,$pixels)`，`src/styles/functions/rem.scss`），
  全项目无其他组件用过它；误写单参数 `rem(0.5)` 会报 `Missing argument $pixels`（`7b592f2d`）。
- **中文 IME**：`InputService.cpp` WndProc 加 `WM_IME_*`，`GCS_RESULTSTR` 经
  `ImmGetCompositionStringW` 取结果串，逐 UTF-16 code unit 走 `ProcessKeyboard(...,KEYEVENT_CHAR,...)`；
  组合期间 `s_imeComposing=true` 抑制 WM_INPUT 的 ToUnicodeEx。链接用 `#pragma comment(lib,"imm32.lib")`。
- **往 `if/else if` 链里插新事件类型前先确认落点**：`if (aType != X && ...) {} else if (active) {}`
  会让 `X` 静默掉进 `else`（实例 `InputService.cpp::ProcessKeyboard` 的 `KEYEVENT_CHAR`）。
- **字符事件要发"产生的文本"而非虚拟键**（`5cbe1720`），且不要发未初始化的字符（`d912fc54`）。

## 7. 日志分诊速查

- **`cef_debug.log` 不能默认是本站的**（CEF 示例/Adobe CEP/MS Dynamics 都用这名字）。
  **判据不是文件名里的模块名**：CEF 编了 `//chrome` 那一层（`chrome/browser/chrome_browser_main_win.cc`
  等），所以 `chrome\browser\*`、`RunDeElevated`、`Windows.AutoDeElevateResult` 出现在**本站自己的**
  `cef_debug.log` 里完全正常（§13 就是靠这些行抓到的；旧版判据把这几条判成"非本站"，是错的）。
  可靠判据：**文件位置**（游戏根目录 `logs/`，或 `st_boot.log` 记的游戏根）+ **时间戳与
  `tp_client.log` 对得上**。本站 CEF = 官方 `cef 141.0.11`（`Libraries/TiltedUI/xmake.lua:9`）。
- **从日志地址反查符号**：同份日志里找一条已带"模块名+偏移"的行拿模块基址
  （如 `SkyrimSE.exe+0xc02260 (0x7ff73a442260)` ⇒ 基址 `0x7ff738840000`），**不要用崩溃地址反推**。
  再 grep `versionlib-ae-to-se-<ver>.map`（`<AE id> <SE RVA>`）反查。
- `HookAudit` 三条 ship 级结论：**`0 did not land` 不等于挂载成功**（未映射 ID 会挂到共享空桩，
  另有专门 error 行提示）；重复 target 说明两个 id 落同一地址；共享 `ff 25` 说明别的 mod 也在钩。
- 版本核对：凡"某版本还是不行"，先核对日志时间戳是否**晚于**该 release 发布时间。
- 同 mod 内重复钩子是上游继承缺陷，**不要擅自删**（`TESObjectREFR.cpp` id 19708 与
  `Actor.cpp` id 37525 同为 `HookAddInventoryItem`，1.5.97 里两个 id 都映射到 `0x28e680`）。
- 游戏主线程是**纯事件驱动**打日志（加载 ~450 行/秒、空闲 ~0.05 行/秒）。"长时间无日志"≠ tick 停了，
  要证明 tick 存活得靠专门的仪器化心跳，不能只看有没有日志。
- **MO2 的 `mo_interface.log`/`usvfs-*.log` 是 MO2 自身日志，不含崩溃信息**；正确诊断文件是
  游戏根目录 `logs/tp_client.log`、`crash_*.dmp`、`st_client_error.log`、`st_boot.log`、`cef_debug.log`。

## 8. 崩溃报告与地址库自身缺陷（`e73f52c9` 修过一轮）

- **`CrashHandler.cpp` 崩溃 hexdump 在"跳空"时为空**：`ExceptionAddress - 0x200` 在地址 < 0x200 时
  无符号回绕 → `SafeReadBytes` 返回 0 → 最该看清现场的那类崩溃（rel32 跳空）反而打不出字节。
  修法：`>= 0x200` 才回退，否则从地址本身开始。
- **转储失败被记成成功**：`CreateFileA` 失败返回 `INVALID_HANDLE_VALUE`(-1) 而非 NULL，
  `if (!hDumpFile)` 判不出来。要对 `INVALID_HANDLE_VALUE` 判定 + 检查 `MiniDumpWriteDump` 返回值。
- **`VersionDb.h` 截断的地址库会崩在读库里**：`read<T>()` 返回未初始化栈值且从不看流状态 →
  空/截断文件可能碰巧解析成 format 1/2/5 继续跑；`ptrSize` 为 0 时整数除零直接打掉进程。
  修法：值初始化 + `ptrSize` 限 4/8 + `addrCount` 限界 + `file.eof()` 早停 → 干净 `return false`。

---

## 9. 已修复 bug 清单（按 commit 时间线，hash 可追溯）

| commit | 版本 | bug / 修复 |
|---|---|---|
| `c819d684` | v1.0.18 前 | F2 开关与 overlay 页面状态脱钩（`m_active` vs JS `active$`/`inGame$`），F2 只见光标 |
| `d374a1a5` | — | 合并上游 TiltedEvolution dev（18 冲突文件），引入后续一串回归 |
| `9da3a2ac` | — | 合并引入/暴露的 4 个缺陷 |
| `ec4e828f` | — | `OnNotifySubtitle` 缺 `isLeader` 声明（编译错） |
| `b15c3284` | — | 合并遗留 3 处不一致（xmake 版本对账、fomod 漏装 QuestPatches.esp 等） |
| `66d61839` | — | `.gitmodules` 幽灵 submodule 残留 |
| `c03200fc` | — | `VersionDb.h` 被合并"半截拼接"（−64 行）致客户端构建失败 |
| `f1ef89d2` | — | `InventoryService` 的 `isLeader`（第三处）被合并吃掉 |
| `9a20cf20` | — | 裸体 NPC 自愈片段被合并吃掉 |
| `d810d3da` | v1.0.19 | FOMOD 安装引导加作者/项目地址 |
| `00ec7306` | — | **rel32 越界截断**（P0）：运行时 DLL 离镜像 ~20GB，字节补丁跳进未映射内存 |
| `f361242e` | — | FOMOD 引导的 `fomod/info.xml`（作者/版本/网站） |
| `3ba69aa5` | v1.0.20 | 过时文档整理（RELEASE-AND-MO2 等） |
| `c0abe640` | — | **锚点式 pad 重复计算**：新游戏闪退，`locationForm` 读成邻居 |
| `7e154b06` | v1.0.21 | 记录 1.5.97 审计基线 |
| `e73f52c9` | v1.0.23 | CrashHandler hexdump 回绕 + 转储误报成功 + VersionDb 截断除零 |
| `66851aaf` | — | 连接握手加 10s 看门狗 + 取消真正中止 + 先 Close 再 Connect |
| `be53b6cd` | v1.0.24 | 取消的握手上报为 disconnect |
| `f9b81d21` | v1.0.25 | 错误清掉 overlay 的 connecting 状态 |
| `b0082844` | v1.0.26 | `SkyrimVM::inactive` 改读 1.5.97 真实偏移 |
| `83f3f89e` | — | 失败连接尝试上报为 disconnect（**去重标志误合并，后被 6acd6ef8 修**） |
| `6acd6ef8` | v1.0.27 | 拆 `m_attemptErrorReported`/`m_attemptDisconnectReported`；修复"自动重连死代码" |
| `b7abf355` | v1.0.28 | 修正 ProcessConnectMessage 上方注释；不碰内存动作移出 Runner 队列 |
| `5efab47f` | v1.0.29 | 连接尝试拥有独立 pump（handshake 专用线程 + recursive_mutex） |
| `d9f01e7e` | v1.0.30 | pump 只解析不分发，解析结果交还游戏线程（消除跨线程碰游戏内存） |
| `f30b30c4` | — | 连接结果弹窗 + 菜单常显 + 中文 IME 输入 |
| `7b592f2d` | v1.0.31 | UI 构建失败：`rem()` 少传参数 |
| `a9ad774f` | v1.0.32 | 帧循环改 WM_TIMER 驱动（1.5.97 两个帧钩子从不执行） |
| `83bb37be` | **未打 tag** | **启动闪退**：定时器无门控，启动期提前跑 `Update()`；补 `GetInGame()` 门控 + 判空 |
| `45577fa4` | v1.0.34 | **4 条 1.5.x id 映射错误**（§11）：`400475` 指到 TimeData、`37525`/`19708` 撞同一地址（就是那条 `already claimed`）；生成器 join 键改 `(class, symbol)` + 拒绝写重复表 |
| `3197edde` | v1.0.35 | **`SkyrimVM::virtualMachine` 偏移**（§12.3）：legacy 应为 `0x200` 而非上游的 `0x210`；按目标分叉 + 各自 `static_assert` |
| `f11dbc26` | v1.0.36 | **CEF 在提权进程里自动去提权**（§13）：退出码 `38`，F2 只见光标、菜单永不出现；**并且多拉出一个原版 `SkyrimSE.exe`**（§13.2，同一根因）；`OnBeforeCommandLineProcessing` 追加 `do-not-de-elevate`，并把退出码翻译成人话 |

## 10. 未修复 / 遗留问题（open，按优先级）

- **【P1，已结案，见 §11】**0x921F10 / 0x5B2FF0 **没有贴错**，是真实 1.5.97 地址；两个钩子不执行
  是因为 SKSE 路径下游戏主循环由 PE 入口直接进入，而该入口不经过被钩的那条调用链。结论已定，
  不要再"修"这两个 id。
- **【P1，已结案，见 §11】**`hook target ... already claimed` 的真身是**映射表把两个 id 指到同一
  地址**（`37525`/`19708` 都落 `0x28e680`），不是 `RipAllocateN` 桩冲突。已修表 + 增强日志。
- **【P1，已观测未触发，见 §12.5】传输泵兜底分发**：tick 彻底不来的会话里，handshake pump 超 500ms
  会自己分发（`frame loop did not take the queued messages within 500ms` warn），**会碰游戏内存**。
  v1.0.34 两场实机会话里**一次未触发**：WM_TIMER 让帧循环始终活着，pump 一发现
  帧循环在就交还。保留现状，**但别删**——它是 tick 真死时唯一能连上的路径。
- **【P2】F3 调试菜单在 1.5.97 走不通**：`DebugService::OnUpdate` 依赖 `UpdateEvent`，而 1.5.97
  上 UpdateEvent 曾经从不触发（现已由 WM_TIMER 驱动，待复测）；且 release 下 F6/F7/F8 被 `IS_MASTER` 裁掉。
- **【P2】i18n 缺键**：es/fr/nl/pl/zh-CN 部分词条缺失。
- **【P2，上游继承缺陷，fork 已修但上游未修】自动重连死代码 + `DisconnectedEvent` 空结构体**：
  上游 `dev` 至今仍是 `ExecuteAsync("disconnect")` 无参、`DisconnectedEvent.h` 空结构体，同步时勿丢 fork 修复。
- **【P3】版本握手强制同 commit**：`TransportService.cpp:115` 发 `Version=BUILD_COMMIT`，
  `GameServer.cpp:856` 要求 `== BUILD_COMMIT` 否则 `kWrongVersion` 踢出。联机双方必须同版本构建。
- **【已修，见 §11 + §12.3】`SkyrimVM::Get()`（id 400475）启动期读到 null**：两个独立缺陷叠加。
  ① 映射表把 `400475` 指到了 `TimeData::s_instance` 的 `0x1ec0a80`（§11，已修）；
  ② `SkyrimVM::virtualMachine` 在 1.5.97 上是 `0x200` 而不是上游的 `0x210`（§12.3，已修）。
  启动期读到 null 本身是正常的（VM 单例尚未建立）。
- **【观察项】客户端日志 0 字节**：曾出现从非 SKSE bootstrap 路径启动（launcher）导致
  `st_boot.log` 为空的情况，排查前先问清"这次是怎么启动的"（MO2 / 直接 SKSE / launcher）。

---

## 11. 1.5.97 地址库审计（2026-09-22，对着真实 `SkyrimSE.exe` 1.5.97.0 做的）

**方法**：不用猜测、不用工具链。三份独立证据交叉：
① 真实 1.5.97 `SkyrimSE.exe`（34,769,792 字节，`FileVersion 1.5.97.0`）的 PE 反汇编；
② 仓库自带的 `version-1-5-97-0.bin`（778,674 条 SE id）与 `versionlib-1-6-1170-0.bin`（428,461 条 AE id）；
③ `git tree @ 8eaca858^`（AE 迁移前一版，**硬编码 1.5.97 绝对地址**）这是本仓库自己的真值。

`8eaca858` 的 diff 是可直接对账的：`- 0x141EC3B78` → `+ 0x141F5E378`，符号名不变。
把两侧按 **文件名 + 符号名** join（不是只按符号名），就是一张 1.5.97 真值表。

### 11.1 结论一：`0x921F10` / `0x5B2FF0` **没有贴错**

两个地址在真实二进制里就是它们声称的函数，且互为上下游：

```
WinMain 0x5ACBD0
  └─ call 0x5AF3D0        帧循环：PeekMessageA/TranslateMessage/DispatchMessageA + Sleep(0x32)
       └─ call 0x5B2FF0   （循环体内，每帧一次）
            └─ call 0x921F10   rcx = [0x141EC3B78]（SkyrimVM 单例）
```

- `0x921F10` 入口 `mov rax,rsp / mov [rcx+0x750],1`，体内 `movzx r15d,[rsi+0x680]` 
  与 1.6.1170 的 `SkyrimVM::Update`（`ae id 53926`）同形，`+0x680` 正是 1.5.97 的 `inactive`
  （与 §2 的 `GameVM.h` 断言一致）。**这个地址是对的。**
- `0x5B2FF0` 在 1.5.97 里是 `0x5AF3D0` 帧循环体内的每帧调用，`0x921F10` 的唯一调用者。
  与 1.6.1170 的 `ae id 36564` 相对位置一致（两边都是"帧循环里的每帧钩子"）。**这个地址也是对的。**

**那为什么心跳整场为 0？** 因为 `0x5ACBD0` 这条链**只从 PE 入口 `0x134B228` 走**
（`entry → 0x134B05C → 0x5ACBD0`）。而 **launcher 路径是 `loader.GetEntryPoint()` 直接跳游戏入口，
SKSE 路径则由 SKSE 自己驱动游戏主循环**两条实际路径都不经 `TiltedOnlineApp::GetMainAddress()`
（它用的是另一个 id `36544 → 0x5ACBD0`，**全仓无人调用**）。

> 教训：**"钩子 0 did not land" ≠ 地址错**。地址正确、字节也写进去了，函数只是不在实际执行的
> 路径上。判定"贴错"之前必须先证明该函数在真实调用链里；否则会把对的地址改成错的。
> 这也是 WM_TIMER 方案（§5.6）能站住脚的原因：它绕开的是"谁驱动帧循环"，不是"地址对不对"。

### 11.2 结论二：`already claimed` 的真身 = 映射表把两个 id 指到同一地址

`hook target 0x7ff739ace680 already claimed` 反推的 RVA 是 `0x128e680`（基址 `0x7ff738840000`，
由同日志另一条带模块名的行得到）。**该 RVA 不在 1.5.97 库里**，所以当时怀疑 `RipAllocateN` 桩冲突。

真实原因是**映射表自己的重复**：`versionlib-ae-to-se-1-5-97-0.map` 里两个 id 指向同一偏移，
两个钩子于是落在同一函数上，`HookAudit::Record` 报"已被占用"。

1.5.97 地址库本身**零重复**（778,674 条 id ↔ 778,674 个互异偏移），所以映射表里任何重复都是**错**的：

| 偏移 | 被哪两个 id 认领 | 后果 |
|---|---|---|
| `0x28e680` | `19708`（TESObjectREFR::AddInventoryItem）、`37525`（Actor::AddInventoryItem） | **就是那条 `already claimed`**；Actor 侧钩子落到了 REFR 的函数上 |
| `0x54cb70` | `34140`（ActorMagicCaster::InterruptCast）、`34408`（MagicCaster::InterruptCast） | 同上，钩错函数 |
| `0x1ec0a80` | `400447`（TimeData::s_instance）、`400475`（SkyrimVM::Get 单例） | **`SkyrimVM::Get()` 读到 TimeData**，直接踩坏 §5.5 的整条更新链 |
| `0x2f27188` | `403350`、`403560` | 两者代码库都未引用。但地址库是单射的，重复即错，已删 `403350`（见 §11.5） |

两个库都是**单射**（`1.5.97` 778674 条 id ↔ 778674 个互异偏移；
`1.6.1170` 428461 ↔ 428461），所以正确的 id 映射也必须单射——
**重复就是 bug，不是风格问题**。

### 11.3 结论三：另外两处也错了（真值表比对发现）

按 **(文件, 符号)** join 对账 114 个同名点，除上述外还有 2 处不符真值：

| ae_id | 符号 | 错值 | 真值 | 真值依据 |
|---|---|---|---|---|
| `14953` | `TESTexture::Construct` 的 ctor | `0x938b40` | `0x1a0bc0` | 快照 `0x1401A0BC0`；`8eaca858` diff 同符号 `0x1401A0BC0` → `0x1401AC180` |
| `400475` | `SkyrimVM::s_instance` | `0x1ec0a80` | `0x1ec3b78` | 快照 `0x141EC3B78`；且 4 个连续 8 字节槽在两边一一对应 |

`400475` 的判定尤其干净——1.6.1170 与 1.5.97 的这段是**等距**的：

```
ae 400473 0x20fba60 ──0x18──> ae 400476 0x20fba78      (1.6.1170)
se 514313 0x1ec3b68 ──0x18──> se 514316 0x1ec3b80      (1.5.97)
                        
            400475 必须落 0x1ec3b70 槽；表里却写 0x1ec0a80（se_id 514287）
```

### 11.4 根因：生成器的 join 键是**裸符号名**，而符号名会重名

`Tools/Scripts/gen_se_map_from_history.py` 把两侧都塞进 `{符号名: 值}` 字典，**后写覆盖先写**。
而 `s_instance`、`s_constructor`、`s_addInventoryItem`、`s_interruptCast`、`ctor`、`s_start`、
`s_equipFunc`、`s_unequipFunc`、`s_singleton` 这些名字在树里**各属于多个类**：

- `s_instance` 在 1.5.97 快照里有 **7 个互异地址**（UI / SkyrimVM / TimeData / WeatherManager /
  Renderer / FormManager / QuestCallbackManager）；
- `s_addInventoryItem` 有 2 个（Actor `0x5e6f20`、TESObjectREFR `0x28e680`）；
- `s_interruptCast` 有 3 个。

join 时只能留下一个，其余全错。**这是"贴错"的真正来源**，而不是历史符号 join 本身不可信。

> **硬性规则（后续同步/重生成地图时必守）**：
> 1. join 键必须是 **(文件, 符号)**，不是裸符号名；
> 2. 生成后必须过两道断言：**映射表内偏移不得重复**（1.5.97 库零重复）、
>    **相邻 AE id 的映射结果不得严重逆序**；
> 3. 改完 `PlayerCharacter.h` 那类锚点偏移后，同一批 id 要重跑这两道断言。

### 11.5 不只是 1.5.97：10 张表全中

生成器用 `--se-bins-dir` 把 1.5.97 的结果**链式**推到其余 1.5.x
（`ae id → 1.5.97 rva → se id → 该版本 rva`），所以这 4 个错误
**10 张表全有**，只是偏移各不相同：

| 版本 | `37525` 错值 | `34140` 错值 | `400475` 错值 | `14953` 错值 |
|---|---|---|---|---|
| 1.5.3 | `0x28e750` | `0x54b610` | `0x1eda000` | `0x9372b0` |
| 1.5.16 | `0x28e7d0` | `0x54cab0` | `0x1ee6a00` | `0x938760` |
| 1.5.23 | `0x28e800` | `0x54cae0` | `0x1ee6a00` | `0x938790` |
| 1.5.39 | `0x28e8e0` | `0x54d020` | `0x1ee7a80` | `0x938ff0` |
| 1.5.50 / 53 / 62 | `0x28e870` | `0x54cd60` | `0x1ee7a80` | `0x938d30` |
| 1.5.73 / 80 | `0x28e680` | `0x54cb70` | `0x1ec0a80` | `0x938b40` |
| 1.5.97 | `0x28e680` | `0x54cb70` | `0x1ec0a80` | `0x938b40` |

另外删掉一条**无法定值**的：`403350` 与 `403560` 同报 `0x2f27188`。
`403560` 是对的——它与 `403559`/`403566`/`403567`/`403568` 在两个库里都是紧凑 8 字节串，
且 `403560→403566` 在两边都是 `0x30`（`0x3187780→0x31877b0` 与 `0x2f27188→0x2f271b8`）。
`403350` 则无法定值：它在 1.6.1170 的 `0x3186d98` 周围全是未映射的，
括号两端（`403340→0x2f26740`、`403447→0x2f26b7c`）之间有 78 个候选槽位。

故**删除而不猜**：未映射会退化到共享零返回 stub 并在日志里**喊出来**，
而错误映射是**静默指向邻居函数**——正是本次审计要消灭的那类失败。

修法：先把 1.5.97 修对，再用**同一条链**重算其余 9 张
（正确的 se id 在 1.5.x 内是稳定的，这正是生成器本来就依赖的性质）。
修完 40/40 条新值均能在**对应版本自己的** `version-1-5-*.bin` 里找到。

### 11.6 本次改动

| 文件 | 改动 |
|---|---|
| `GameFiles/.../versionlib-ae-to-se-1-5-*.map`（**10 张**） | 各修 4 行（共 40 行） |
| `Tools/Scripts/gen_se_map_from_history.py` | join 键改为 **(class, symbol)**；新增两道断言：**偏移不得重复**、相邻 id 不得大量逆序；不通过则**拒绝写文件** |
| `Tools/ida/st_overrides_all.tsv` | 修正同样 4 条，并在表头记下原因（它是重生成时的输入，不改会复发） |
| `Code/client/HookAudit.cpp` | 碰撞 error 补 `FormatModuleOffset`（模块+偏移），并在两处打印 **ae id** |
| `docs/PITFALLS.md` | §10 两条 P1 结案；本 §11 |

`HookAudit` 现在会把 `hook target SkyrimSE.exe+0x28e680 (0x..., id 19708) is already claimed`
一次说清“哪个函数、哪个 id”，不必再靠反推基址。

### 11.7 复核用的命令（照抄）

```powershell
# 1. 真值表：AE 迁移前的硬编码 1.5.97 地址
git show "8eaca858^:Code/client/SkyrimVM64.cpp" | Select-String POINTER_SKYRIMSE

# 2. 映射表里有没有重复偏移（1.5.97 库零重复 ⇒ 重复即错）
python -c "import collections;m={};
[m.__setitem__(int(l.split()[0]),int(l.split()[1],0)) for l in open('GameFiles/Skyrim/SKSE/Plugins/versionlib-ae-to-se-1-5-97-0.map') if l.strip() and not l.startswith('#')];
r=collections.defaultdict(list);[r[v].append(k) for k,v in m.items()];
print({hex(v):i for v,i in r.items() if len(i)>1})"

# 3. 反汇编某个 RVA（VA = 0x140000000 + RVA）
& D:\clangllvm\bin\llvm-objdump.exe -d --start-address=0x140921f10 --stop-address=0x140921f80 `
    --x86-asm-syntax=intel "<游戏目录>\SkyrimSE.exe"

# 4. 谁调用某函数（.text 里扫 call rel32）
#    见 §11.1 的调用链；同理可对任意 RVA 求调用者
```

---

## 12. v1.0.34 实机联机日志分析（2026-09-23，1.5.97 + MO2 + 整合包）

日志：`tp_client.log` 4485 行，两次会话（00:01–00:04 与 00:13–00:20），
均 `authentication accepted`，第二次跑满 7 分钟无崩溃。

### 12.1 §11 的修复被实机证实

| 验证项 | 结果 |
|---|---|
| `already claimed` 重复钩子 | **0 次**（v1.0.33 前每次会话必现） |
| `id 37525` 落点 | `SkyrimSE.exe+0x5e6f20` ✅ 正是 §11.3 修的值 |
| `id 19708` 落点 | `SkyrimSE.exe+0x28e680` ✅ 未受影响 |
| 钩子汇总 | `63 recorded, 0 did not land` ✅ |
| 同步功能 | 本地角色指派 257、远程角色 257、物体 322、所有权转移 217 |

### 12.2 §11.1 的结论被实机证实

**`vm tick heartbeat` 与 `main loop heartbeat` 在整个 4485 行日志里一次都没有出现**，
而 `timer-driven update heartbeat` 有 111 次采样（tick 到 22500）。这正面证实了 §11.1：

> 那两个地址是**对的**，函数只是不在实际执行的路径上。WM_TIMER 驱动是必需的，不是权宜之计。

### 12.3 新发现：`virtualMachine` 在 1.5.97 上不是 `0x210`，是 `0x200`

日志每次会话都报一次：

```
the SkyrimVM singleton at 0x7ff7377a7e80 holds the papyrus vm at +0x200,
not the +0x210 this build assumes; every other offset into this struct is
suspect for the same reason
```

**根因**：与 §2 的锚点式 pad 同一类错误。`v1.0.18`（fork 定版基线）用的是 `0x200`，
合并 `d374a1a5` 时按"adopt upstream value"规则改成了上游的 `0x210` ——
但 `0x10` 正是这个结构体在 AE/SE 之间的差值（`inactive` 同样是 `0x680` vs `0x690`），
**两个目标必须各定各值**。

**独立证据**（不依赖日志，来自真实 1.5.97 二进制）：

1. `SkyrimVM::Update`（`0x921F10`）自身 **5 处**读 `[rsi+0x200]`，其中一处带 null 检查；
2. 紧随其后被调用的 `0x922A90` 读 `[rcx+0x200]` 再 `jmp [rax+0x28]` ——
   正是对该指针的虚表调用；
3. `v1.0.18` 基线即 `0x200`。

**为什么一直没炸**：`SetVirtualMachine()` 会用游戏递给 papyrus 钩子的指针覆盖静态量，
单例字段只是"钩子还没跑时"的 fallback；读错偏移得到的是非空但不像 vm 的指针，
被 `LooksLikeVirtualMachine()` 拦掉 —— 于是 papyrus 路径**静默降级**，
而不是崩溃。修法：按 `SKYRIM_TARGET_LEGACY` 分叉，legacy `0x200` / AE `0x210`，
并各自 `static_assert`。

> **教训（再次）**：上游的"修正"是**对 AE 而言**的修正。fork 同时供两个目标时，
> 任何被上游动过的偏移都要先问一句"这是绝对值还是相对增量"，再决定跟不跟。
> 本次与 §2 的 `PlayerCharacter::pad1`、§5.6 的 `SkyrimVM::inactive` 是**同一个坑的第三次**。

### 12.4 日志里其余噪声的分诊（都不是 fork 的 bug）

| 现象 | 判定 |
|---|---|
| `Failed to create Discord instance (4)` ×2 | Discord 未运行。非致命，已有处理 |
| `multiple behavior replacers have the same signature` ×27 | **用户整合包装了两个动画 mod**（Cow/Deer/Goat/Horker/Horse/SabreCat/Skeever/Wolf 各被声明 2 次）。代码已优雅降级（`choosing the first one`），非 fork bug |
| `patch 'window style' skipped` / `patch 'skip startup movie' skipped` | 设计如此：无 1.5.x 验证偏移就不打（§4） |
| `id 36548 is not mapped` | 已知未映射项，见 `Tools/missing_1_5_97_ids.txt` |
| `hook target SkyrimSE.exe+0xc02260 already held a branch (ff 25 ...)` ×1 | **别的 mod 也在钩同一函数**（id 68115），`HookAudit` 正确识别并报告。属预期，非 bug |
| `BehaviorVar::LoadReplacerFromDir` 数百行 | 正常加载日志，info 级 |

### 12.5 P1 遗留项：传输泵兜底分发**从未触发**

§10 记的"handshake pump 超 500ms 会自己分发、会碰游戏内存"，本次两场会话
`did not take the queued messages` **出现 0 次**。原因是 WM_TIMER 让帧循环始终活着，
pump 一发现帧循环活着就立刻交还（日志里可见
`handshake pump: frame loop is live, handing ... back to it`）。

> 结论：该风险只在"帧循环彻底不来"时成立，而当前构建已不存在这种会话。
> 保留现状，但**别删**这条兜底 —— 它是 tick 真死时唯一能连上的路径。

### 12.6 复核用的命令（照抄）

```powershell
# 有没有重复钩子（应为 0）
Select-String -Path tp_client.log -Pattern "already claimed"

# 两个帧钩子是否真的从不执行（应为空）
Select-String -Path tp_client.log -Pattern "vm tick heartbeat|main loop heartbeat"

# 定时器驱动是否活着（应有大量采样）
Select-String -Path tp_client.log -Pattern "timer-driven update heartbeat" | Measure-Object

# 兜底分发是否触发过（应为 0）
(Select-String -Path tp_client.log -Pattern "did not take the queued messages").Count

# virtualMachine 偏移告警（修好后应为空）
Select-String -Path tp_client.log -Pattern "holds the papyrus vm"
```

---

## 13. v1.0.36 日志分析：CEF 自动去提权（F2 只见光标，菜单永不出现）

**现象**：`D:\sktest\new`（2026-09-14 ~ 09-20 共 10 场会话）每场都是 `renderer init` 之后立刻：

```
Overlay could not be initialized
CEF failed to initialize, exit code 38. See 'cef_types.h' for description
```

之后 `overlay render pump is live`、`overlay in-game state: true` 照常打印，按 F2 也**只有软件光标**
（光标是 D3D11 sprite batch 每帧画的，与页面死活无关，所以它"有反应"恰恰说明不了问题）。
菜单永远出不来。

**根因**：`38` = `CEF_RESULT_CODE_NORMAL_EXIT_AUTO_DE_ELEVATED`（`cef_types.h`）。
Chromium 在**浏览器进程**里检测到"进程已提权、且 UAC 开着"就重新拉起一个未提权的自己，
然后**让当前进程退出**：`chrome_browser_main_win.cc::MaybeAutoDeElevate`，
判据是 `base::win::UserAccountIsUnnecessarilyElevated()`（= `TokenElevationTypeFull`）。

游戏这次是**以管理员身份**启动的，于是 CEF 把 `SkyrimSE.exe` 又拉了一份、自己退出：

- 新进程拿不到游戏的 D3D11 设备/swapchain/窗口，overlay 页面永远建不起来；
- 老进程里 `CefInitialize()` 返回 false，`m_pOverlay` 没有页面，只剩渲染泵和光标在跑。

**为什么拖了一周才找到**：`cef_debug.log` 里其实写着 `RunDeElevated: Started process, PID: 5848`，
但 §7 旧的"排除判据"把 `RunDeElevated`/`Windows.AutoDeElevateResult` 判成"非本站日志"
（理由：本站不编 `//chrome`）——**这个理由本身就是错的**，CEF 编了 chrome 那一层，这些行就是本站的。
判据已改（见 §7）。

**修法**（`Code/client/Services/Generic/OverlayService.cpp`）：把 `OverlayApp` 派生一层
`SkyrimTogetherOverlayApp`，在 `OnBeforeCommandLineProcessing` 里追加 `do-not-de-elevate`。
它是 Chromium 的 `kNoRestartSwitches` 之一，**在"要不要重拉"判断之前**就被检查，
所以 CEF 留在原进程里跑，页面才建得起来。顺带把退出码翻译成人话，`38` 直接点名"去提权"。

> **为什么不顺着 CEF 让它去提权**：重拉的目标是 `SkyrimSE.exe` 自己，而 D3D11 设备、swapchain、
> 窗口句柄都不跨进程；新进程不可能接管游戏画面，只会多一个空转的 SkyrimSE。

> **教训**：日志排除判据必须能拿**证据**站住，不能靠"我们没编那个模块"的推测。
> 一条错的判据会把最该看的文件判成噪声，代价是 10 场会话。

### 13.1 复核用的命令（照抄）

```powershell
# 是不是被去提权（有这两条就是）——注意它出现在本站自己的 cef_debug.log 里
Select-String -Path cef_debug.log -Pattern 'RunDeElevated|AutoDeElevateResult'

# 客户端侧的证据：初始化失败 + 退出码
Select-String -Path tp_client.log -Pattern 'CEF failed to initialize'

# 确认游戏进程真的提权了（以管理员跑 PowerShell 才会是 True）
([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
```

### 13.2 同一个 bug 的另一张脸：多出来的那个 `SkyrimSE.exe`

用 MO2 的 `SkyrimTogether.exe` 入口启动时，任务管理器里会**同时出现两个进程**，
关掉 `SkyrimSE.exe` 那个之后游戏照常跑。这不是两个游戏，是同一个 bug 的另一半：

1. 入口是 MO2 的 6 号项 `mods/Skyrim Together Next/launcher/SkyrimTogether.exe`（immersive launcher）。
   它**不启动** `SkyrimSE.exe`，而是把游戏映像映射进自己的进程（`ExeLoader`），
   所以任务管理器里那个进程叫 `SkyrimTogether.exe`，但它**就是**游戏本体。
2. 为了骗过游戏和 mod，launcher 把 `GetModuleFileNameW(NULL)` / `LdrGetDllFullName` 挂钩，
   让它们返回**游戏真实路径**（`Code/immersive_launcher/stubs/FileMapping.cpp`）。
3. Chromium 的 `MaybeAutoDeElevate` 用 `base::PathService::Get(base::FILE_EXE)` 取"自己"的路径 ——
   它落到 `GetModuleFileName(NULL)`，**正好踩中这个挂钩**，拿到 `D:/game/SkyrimSE/SkyrimSE.exe`。
4. 于是 `CreateProcessWithTokenW` 拉起的不是 launcher，而是**一个干净的、没挂钩的原版 `SkyrimSE.exe`**。

所以那个多出来的进程是"原版游戏自己"：没有 launcher、没有 usvfs、没有联机。关掉它当然不影响主进程。
`AllowSetForegroundWindow failed: Access is denied (0x5)` 就是这一步的残响（§13 的日志第 4 行）。

> **一个根因，两个症状**：`do-not-de-elevate` 一旦生效，重拉这一步根本不发生，
> **两个现象一起消失**。所以"多一个 SkyrimSE"和"F2 只有光标"不要去分别修。

> **副产品**：这个 bug 还顺手解释了为什么"以管理员运行"会看起来时好时坏 ——
> 提权 + UAC 开启才触发；不满足条件时 Chromium 直接放行，一切正常。
