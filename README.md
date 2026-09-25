[English](README_EN.md) | 中文

# Skyrim Together Next

[![Build windows](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml/badge.svg)](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml)
[![Discord](https://img.shields.io/discord/247835175860305931.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/skyrimtogether)

> ## ⚠️ 推荐游戏版本:**1.5.97**
> ## 📦 推荐 Mod 版本:**>= 1.0.41**
>
> 1.5.97 是本框架验证最充分的版本,1.0.41 起修复了部署与联机崩溃问题。
> 使用其它游戏版本或更旧的 Mod 版本出现的问题,请先升级到上述版本再反馈。

为 Bethesda 游戏提供联机能力的开源框架,当前支持 **上古卷轴 5:天际特别版(Skyrim Special Edition)**,也就是广为人知的 **Skyrim Together**。

本仓库在上游 [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution) 基础上扩展了多项实用能力:

## ✨ 本仓库特性

- **多游戏版本支持**:1.6.1170 / 1.6.640 等 1.6.x 全系、1.7.x 新版本(1.7.99/1.7.104)、以及**老版 1.5.x(含 1.5.97)**,地址库与 ID 映射表全部随包附带;
- **MO2 + SKSE 无缝启动**:自带 SKSE 插件,通过 Mod Organizer 2 正常启动 `skse64_loader.exe` 即可加载本 mod,并带**自部署运行时**——首次启动自动把所需文件部署到游戏根目录,无需任何手动复制;
- **图形化安装引导(中英双语)**:mod 包内置 FOMOD 安装向导(MO2 原生支持),第一步先选中文/English,后续步骤与文案随之切换;联机所需文件自动安装,只把真正需要你选择的东西交给你勾选——两个框架选项(1.6.x 的启动校验脚本、独立启动器),外加**三个可选插件**,每个都带一句说明,一个都不装也不影响联机;
- **三个可选联机插件内置**:OStimTogether(动作场景同步)、MorphSyncTogether(体型与覆盖层同步)、IEDSyncTogether(装备展示同步)随同一个包分发,在安装向导里按需勾选,不需要单独下载、也不需要自己开放或转发端口;每个插件的附属项(OCum、PPA、体毛覆盖包等)同样在向导里展开;
- **服务器图形控制面板**:Windows 专用服务器带精简 GUI(状态/在线人数/日志/启停按钮),`--nogui` 可回到纯控制台模式;
- **局域网联机开箱即用**:网络层为纯 UDP/GameNetworkingSockets,不依赖 Steam,支持局域网、Radmin LAN、Hamachi、公网 VPS/Docker 等任意组网方式;
- **Release 自动构建**:推送 tag 即在 GitHub 云端完成 Windows 构建并发布两个包——`SkyrimTogetherNextMod-<版本>`(客户端 mod)与 `SkyrimTogetherNextServer-<版本>`(专用服务器)。

> ⚠️ **当前处于发布冻结期**:仓库根目录存在 `RELEASE-FREEZE.md` 期间,**禁止发布新版本**——推送 `v*` tag 或手动发布 Release 都会被 release 工作流直接拒绝。推送普通提交、跑 CI、本地打包演练均不受影响。解除方式见该文件。

## 🚀 快速开始(玩家)

1. 从本仓库 [Releases](../../releases) 下载两个 zip(版本号相同的 Mod 包与 Server 包);
2. **房主**:解压 Server 包,运行 `SkyrimTogetherServer.exe`(图形界面,默认监听 UDP 10578,记得防火墙放行);
3. **所有玩家(含房主)**:把 Mod 包通过 MO2"从文件安装"(会弹出安装向导),或手动解压到游戏 `Data/`;通过 MO2 启动 SKSE 即可;
4. 进游戏按 **F2** 呼出联机界面,填写 `<房主IP>:10578` 连接。

局域网/Radmin 联机图文说明见 [docs/LAN-RADMIN-GUIDE.md](docs/LAN-RADMIN-GUIDE.md)。

## 📦 支持的游戏版本

| 游戏版本 | SKSE | 说明 |
|---|---|---|
| 1.5.3 ~ 1.5.97(老 SE) | SKSE 2.0.x | 完整支持:见下方说明 |
| 1.6.317 ~ 1.6.1179 | SKSE 2.1.x / 2.2.x | 开箱即用(推荐) |
| 1.7.99 / 1.7.104 | 新版 SKSE | 开箱即用(format 5 地址库) |

> **1.5.x 支持说明**:1.5.x(含 1.5.97)已完整支持。AE ID → 1.5.x 偏移翻译表
> 随包附带 10 个版本,地址映射覆盖 **99.7%**(代码引用的 3075 个地址中已解析
> 3066),且全部 3699 条映射都能在官方地址库 `version-1-5-97-0.bin` 中找到对应
> 符号;剩余 9 个(近孪生兄弟函数、无调用者的模块、调试视图)运行时安全降级
> (空实现桩 / RTTI 空指针保护 / 跳过补丁),不会崩溃,清单与各自影响见
> [Tools/missing_1_5_97_ids.txt](Tools/missing_1_5_97_ids.txt)。
>
> **1.5.x 结构体**:包内附带**第二个运行时 DLL**(`SkyrimTogetherRuntime_1_5.dll`),
> 它与 1.6.x/1.7.x 用的那个从同一份源码编出,靠 `SKYRIM_TARGET_LEGACY` 条件编译
> 1.5.x 的结构体布局(相较 1.6.x 普遍相差 8 字节——1.5.x 的 `ExtraDataList` 没有
> 虚表;`TESObjectCELL` 另有三个尾部成员缺失,差 32 字节)。SKSE 引导插件按游戏
> 版本自动挑一个。布局由 `static_assert` 在**编译期**逐个校验,不是"按 1.6.x 编
> 译再赌运行时对不对"。

## ⚠️ 已知冲突的 Mod

以下 Mod 与本框架存在冲突,安装后会引发闪退、界面异常或功能失效,**建议不要同时启用**:

| Mod 名 | 链接 | 中文别名 |
| --- | --- | --- |
| Floating Subtitles | https://www.nexusmods.com/skyrimspecialedition/mods/154424 | 浮动字幕 |

> **Floating Subtitles(po3_FloatingSubtitles)**:该 Mod 的 trampoline 桩跳转到未分配内存页,
> 会在进入房间 / 准备联机时直接导致游戏崩溃。如遇进房闪退,请先禁用此 Mod 再试。
> 若你确实需要字幕类功能,可改用其它实现,或等待上游修复。

## 🧩 支持的整合包

以下整合包已做过联机适配验证,可直接在其基础上启用本框架:

| 整合包名 | 链接 | 稳定性 |
| --- | --- | --- |
| 神话整合 | https://magicskyrim.net/archives/2976 | 测试中 |

> **神话整合**:已通过基础联机流程验证,**稳定性仍在测试中**。遇到闪退请先按
> "反馈问题"附上日志,并说明使用的是该整合包。
> 未列出的整合包并非一定不可用,但未经测试;整合包自带的动画/行为类 Mod
> 若与本框架的行为变量替换冲突,会打印 `BehaviorVar::Patch` 相关日志。

## 🐛 反馈问题

请在仓库的 "Issues" 页面提交,附上可复现步骤、游戏版本、SKSE 版本与服务器日志,详细的报告对开发非常重要。

**日志位置(游戏根目录下):**

| 日志 | 路径 | 说明 |
| --- | --- | --- |
| 客户端日志 | `游戏根目录\logs\tp_client.log` | 联机核心日志:连接、同步、崩溃堆栈都在这里 |
| 界面日志 | `游戏根目录\logs\cef_debug.log` | F2 界面(CEF)相关报错 |

> 多开时客户端日志会依次命名为 `tp_client_instance_2.log`、`tp_client_instance_3.log`;
> 服务器日志为 `logs/STServerOut.log`。反馈闪退问题时请附上崩溃点附近的完整片段。

## 📜 版本更新日志

| 版本 | 日期 | 主要内容 |
| --- | --- | --- |
| **1.1.1** | 2026-09-25 | 三轮审计收尾:修复 `VisitInteriorCell` 在整个 load 期间对空 cell 的链式解引用、`Actor::Create` 对玩家的连续三次解引用、`DebugService` 未判空就用的 actor;并修正 `Actor::Create` 的判空顺序(原会把已分配的 actor 泄漏) |
| **1.1.0** | 2026-09-25 | **性能与同步**:插值改 Catmull-Rom 三次曲线 + 有界外推(消除远端玩家的折线感与丢包时的冻结—跳变);帧循环间隔 16→8 ms,更新率约 32→64/s;移动更新由 O(更新数×实体数) 降为线性;修复重复生成同一远程玩家引发的引擎空指针崩溃;全仓库 `GetById` 解引用审计,修复 15 处无守卫解引用 |
| 1.0.41 | 2026-09-23 | 自部署「假失败」修复:改为内容比对,mtime 仅作前置过滤;删除从未存在的 `.str_old` 清理(它把错误码污染成 `error 2`);定时器量化定量(请求 16 ms 实为 31.25 ms) |
| 1.0.40 | 2026-09-23 | 跟随 `ff 25` 跳转桩,hook 冲突日志直接点名占用方 mod |
| 1.0.39 | 2026-09-23 | 削减每帧开销,解决加载慢与卡顿 |
| 1.0.38 | 2026-09-23 | 全项目审计并移除可证死代码 |
| 1.0.37 | 2026-09-23 | 收尾 F3 / i18n 遗留项,让 F3 复测可复现 |
| 1.0.36 | 2026-09-23 | 阻止 CEF 把 overlay 窗口降级(elevation 丢失) |
| 1.0.35 | 2026-09-23 | 1.5.97 读 `SkyrimVM::virtualMachine` 用 0x200,不是上游的 0x210 |
| 1.0.34 | 2026-09-22 | 修正 4 个错误的 1.5.x id 映射,并让 HookAudit 点名冲突 |
| 1.0.33 | 2026-09-22 | 修复 v1.0.32 的启动闪退:给定时器驱动的帧循环补上门控 |
| 1.0.32 | 2026-09-22 | 帧循环改由窗口定时器驱动,1.5.97 真正能同步 |
| 1.0.31 | 2026-09-21 | 连接结果弹窗、菜单常显、中文输入法、MainLoop 心跳 |
| 1.0.30 | 2026-09-21 | 让网络泵只做解析,分发交还游戏线程 |
| 1.0.29 | 2026-09-21 | 连接尝试自带泵,并把失败原因打出来 |
| 1.0.28 | 2026-09-20 | 从发起连接的那个线程开始连接,而不是 runner 队列 |
| 1.0.27 | 2026-09-20 | 让每次失败的连接尝试都能结束,并通知 UI |
| 1.0.26 | 2026-09-20 | 按 1.5.97 真正的偏移读 `SkyrimVM::inactive` |
| 1.0.25 | 2026-09-20 | 出错时让 overlay 退出「正在连接」状态 |
| 1.0.24 | 2026-09-20 | 连接稳定性:握手超时上限与可用的取消 |
| 1.0.23 | 2026-09-20 | 维护性发布 |
| 1.0.22 | 2026-09-19 | 冲突 mod 清单(Floating Subtitles)与英文 README |
| 1.0.21 | 2026-09-19 | 修复 1.5.97 点击「新游戏」闪退;留档 v1.0.18 兼容性审计 |
| 1.0.20 | 2026-09-19 | 同步上游 TiltedEvolution dev 后的首个构建 |
| 1.0.19 | 2026-09-19 | 合并上游 dev(等级化 NPC 同步 + 版本化归属);修复全部合并引入的编译错误;恢复 1.5.x 兼容与裸体 NPC 自愈;FOMOD 显示作者与项目 |
| 1.0.18 | 2026-09-16 | 修复 F2 overlay 状态;启用 1.5.97 字节补丁 |
| 1.0.17 | 2026-09-06 | 不再在游戏建好 Papyrus VM 之前读它 |
| 1.0.16 | 2026-09-06 | 用 VM 自己的虚表确认 Papyrus hook 打对了函数 |
| 1.0.15 | 2026-09-06 | 日志明确说明 Papyrus 注册 hook 是否执行过 |
| 1.0.14 | 2026-09-06 | 未注册的 Papyrus native 不再拖垮游戏 |
| 1.0.13 | 2026-09-06 | 失效跳转与共享 hook 目标会在日志里自我说明 |
| 1.0.12 | 2026-09-06 | 打通 1.5.97 启动链路;关闭未经验证的字节补丁 |
| 1.0.11 | 2026-09-06 | 缺 payload 时说明原因,而不是归咎权限 |
| 1.0.10 | 2026-09-06 | 修复 1.5.97 崩溃与缺失的 overlay 泵 |
| 1.0.9 | 2026-09-05 | SKSE 启动路径可用;1.5.97 覆盖率到 99.7% |
| 1.0.8.1 | 2026-09-04 | 崩溃处理器补上寄存器、AV 访问地址、代码 hexdump 与栈回溯 |
| 1.0.8 | 2026-09-04 | 去掉 comctl32 ordinal-345 导入(Win11 24H2 报 error 182) |
| 1.0.7 | 2026-09-03 | 用调用图消歧再恢复 5 个 class-A 函数 |
| 1.0.6 | 2026-09-03 | 客户端加载失败时打印各依赖项大小 |
| 1.0.5 | 2026-09-03 | zip 根目录对齐 `Data/`,直接解压安装也可用 |
| 1.0.4 | 2026-09-03 | 从 SKSE 探测真实游戏版本;所有地址库随 Core 附带 |
| 1.0.3 | 2026-09-03 | 用版本字符串而非结构化版本号判定 legacy |
| 1.0.2 | 2026-09-02 | 修正 1.5.x `PlayerCharacter` 内边距锚点(0x580 / 0x588) |
| 1.0.1 | 2026-09-02 | 用 capstone 匹配器把 1.5.x 地址映射补到 99.2% |
| 1.0.0 | 2026-08-30 | 首个版本 |

> 每个 tag 对应一个 [Release](../../releases),附上版本号相同的两个包——
> 客户端 mod 与专用服务器。逐条提交见 `git log`;踩坑记录与定量结论见
> [docs/PITFALLS.md](docs/PITFALLS.md)。

## 🙏 致谢

本项目的存在离不开以下上游与协作项目。它们的成果直接构成本仓库的基础,
在此一并致谢:

| 项目 | 作者 | 本项目如何使用 |
| --- | --- | --- |
| [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution) | Tilted Phoques | **本仓库的上游**。客户端/服务端框架、网络层与引擎逆向成果均来自该项目;GPLv3 许可亦继承于此。 |
| [TiltedEvolution-rwf](https://github.com/rfortier/TiltedEvolution-rwf) | rfortier | **行为/动画 Mod 支持**。`Code/client/ModCompat/BehaviorVar.*` 与随包的 `SkyrimTogetherRebornBehaviors/` 继承自这条 fork 链,详见 [README-ANIMATION-MODS.md](README-ANIMATION-MODS.md)。 |
| [STRPluginMessagingAPI](https://github.com/Caelvanost/STRPluginMessagingAPI) | Caelvanost | **插件消息接口**。对外消费者契约、facade DLL、chat-tunnel 桥与服务器中继资源;本仓库以固定提交的子模块引入,并在 `Code/plugins/STRPM/` 保存权威头文件与契约校验。 |
| [OStimTogether](https://github.com/Caelvanost/OStimTogether) | Caelvanost | **可选联机插件**:动作场景状态、参与者对齐与装备锁同步。 |
| [MorphSyncTogether](https://github.com/Caelvanost/MorphSyncTogether) | Caelvanost | **可选联机插件**:体型滑块与 RaceMenu 覆盖层同步。 |
| [IEDSyncTogether](https://github.com/Caelvanost/IEDSyncTogether) | Caelvanost | **可选联机插件**:装备展示同步。 |

> 三个可选插件与 STRPluginMessagingAPI 均以**固定提交的子模块**引入(`plugins/`),各自保留独立仓库、独立构建与独立发布节奏。本仓库只负责按需打包、接口一致性校验,以及为它们提供联机会话传输;它们的代码与版权归各自作者所有。

## 📄 许可证

[![GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

本项目基于 GPLv3 许可(继承自 Tilted Online / TiltedEvolution),可自由使用、修改与再分发,衍生作品须保持同一许可证。
