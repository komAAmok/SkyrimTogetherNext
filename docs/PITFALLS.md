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
  排除判据（命中任一即非本站）：`Chrome.ProcessSingleton.*`、`Signin.NumberOfActiveAccounts.*`、
  `Windows.AutoDeElevateResult`、`RunDeElevated`（全 Chromium 唯一调用点是 chrome_main_delegate，
  本站不编 `//chrome`）。本站 CEF = 官方 `cef 141.0.11`（`Libraries/TiltedUI/xmake.lua:9`）。
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

## 10. 未修复 / 遗留问题（open，按优先级）

- **【P1，待真实地址库审计】0x921F10（SkyrimVM::Update）与 0x5B2FF0（MainLoop）两个映射地址
  疑似从历史符号 join 时就贴错**：1.5.97 上两个钩子一次都没被调用。目前用 WM_TIMER 绕过，
  但正确做法是用真实 1.5.97 地址库审计这两个 id 到底该落在哪。
- **【P1，待 HookAudit 增强】`hook target 0x7ff739ace680 already claimed by another hook in this mod`**
  反推 RVA `0x12CE680` 不在 1.5.97 映射表，疑似 `RipAllocateN` 桩地址冲突。建议给
  `HookAudit::Record` 这条 error 补 `FormatModuleOffset`（打印模块+偏移）以定位。
- **【P1，诊断性接受的风险】传输泵兜底分发**：tick 彻底不来的会话里，handshake pump 超 500ms
  会自己分发（`frame loop did not take the queued messages within 500ms` warn），**会碰游戏内存**。
  下一步若"能连上但一进世界就崩"，第一嫌疑就是它。彻底修法 = 给会话开一条"只收包不碰游戏"
  的线程 + 世界状态操作 post 回游戏线程。
- **【P2】F3 调试菜单在 1.5.97 走不通**：`DebugService::OnUpdate` 依赖 `UpdateEvent`，而 1.5.97
  上 UpdateEvent 曾经从不触发（现已由 WM_TIMER 驱动，待复测）；且 release 下 F6/F7/F8 被 `IS_MASTER` 裁掉。
- **【P2】i18n 缺键**：es/fr/nl/pl/zh-CN 部分词条缺失。
- **【P2，上游继承缺陷，fork 已修但上游未修】自动重连死代码 + `DisconnectedEvent` 空结构体**：
  上游 `dev` 至今仍是 `ExecuteAsync("disconnect")` 无参、`DisconnectedEvent.h` 空结构体，同步时勿丢 fork 修复。
- **【P3】版本握手强制同 commit**：`TransportService.cpp:115` 发 `Version=BUILD_COMMIT`，
  `GameServer.cpp:856` 要求 `== BUILD_COMMIT` 否则 `kWrongVersion` 踢出。联机双方必须同版本构建。
- **【观察项】`SkyrimVM::Get()`（id 400475）在启动期读到 null**，只导致"布局无法校验"的 warn，
  是否真错位待仪器化确认。
- **【观察项】客户端日志 0 字节**：曾出现从非 SKSE bootstrap 路径启动（launcher）导致
  `st_boot.log` 为空的情况，排查前先问清"这次是怎么启动的"（MO2 / 直接 SKSE / launcher）。
