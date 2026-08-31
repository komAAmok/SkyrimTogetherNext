# Skyrim Together Next

[![Build windows](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml/badge.svg)](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/windows.yml)
[![Build linux](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/linux.yml/badge.svg)](https://github.com/komAAmok/SkyrimTogetherNext/actions/workflows/linux.yml)
[![Discord](https://img.shields.io/discord/247835175860305931.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/skyrimtogether)

Tilted Online 的社区延续版本——为 Bethesda 游戏提供联机能力的开源框架,当前支持 **上古卷轴 5:天际特别版(Skyrim Special Edition)**,也就是广为人知的 **Skyrim Together**。

本仓库在上游 [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution) 基础上扩展了多项实用能力:

## ✨ 本仓库特性

- **多游戏版本支持**:1.6.1170 / 1.6.640 等 1.6.x 全系、1.7.x 新版本(1.7.99/1.7.104)、以及**老版 1.5.x(含 1.5.97)**,地址库与 ID 映射表全部随包附带;
- **MO2 + SKSE 无缝启动**:自带 SKSE 插件,通过 Mod Organizer 2 正常启动 `skse64_loader.exe` 即可加载本 mod,并带**自部署运行时**——首次启动自动把所需文件部署到游戏根目录,无需任何手动复制;
- **图形化安装引导**:mod 包内置 FOMOD 安装向导(MO2 原生支持),分步引导选择游戏版本与联机方式;
- **服务器图形控制面板**:Windows 专用服务器带精简 GUI(状态/在线人数/日志/启停按钮),`--nogui` 可回到纯控制台模式;
- **局域网联机开箱即用**:网络层为纯 UDP/GameNetworkingSockets,不依赖 Steam,支持局域网、Radmin LAN、Hamachi、公网 VPS/Docker 等任意组网方式;
- **Release 自动构建**:推送 tag 即在 GitHub 云端完成 Windows 构建并发布两个包——`SkyrimTogetherNextMod-<版本>`(客户端 mod)与 `SkyrimTogetherNextServer-<版本>`(专用服务器)。

## 🚀 快速开始(玩家)

1. 从本仓库 [Releases](../../releases) 下载两个 zip(版本号相同的 Mod 包与 Server 包);
2. **房主**:解压 Server 包,运行 `SkyrimTogetherServer.exe`(图形界面,默认监听 UDP 10578,记得防火墙放行);
3. **所有玩家(含房主)**:把 Mod 包通过 MO2"从文件安装"(会弹出安装向导),或手动解压到游戏 `Data/`;通过 MO2 启动 SKSE 即可;
4. 进游戏按 **F2** 呼出联机界面,填写 `<房主IP>:10578` 连接。

局域网/Radmin 联机图文说明见 [docs/LAN-RADMIN-GUIDE.md](docs/LAN-RADMIN-GUIDE.md),安装与 MO2 细节见 [docs/RELEASE-AND-MO2.md](docs/RELEASE-AND-MO2.md)。

## 📦 支持的游戏版本

| 游戏版本 | SKSE | 说明 |
|---|---|---|
| 1.5.3 ~ 1.5.97(老 SE) | SKSE 2.0.x | 需勾选 FOMOD 的"旧版支持"选项(装地址库 + ID 映射表) |
| 1.6.317 ~ 1.6.1179 | SKSE 2.1.x / 2.2.x | 开箱即用 |
| 1.7.99 / 1.7.104 | 新版 SKSE | 开箱即用(format 5 地址库) |

> 老版本(1.5.x)的 ID 映射表由本仓库 git 历史中的原始偏移自动生成,覆盖代码中约 97% 的地址引用;个别系统在老版本上仍需实测反馈。

## 🛠️ 开发者

- 构建系统为 **xmake**(C++20),完整构建指南见 [上游 wiki](https://wiki.tiltedphoques.com/tilted-online/technical-documentation/build-guide);
- 代码规范见 [CODE_GUIDELINES.md](CODE_GUIDELINES.md),提交前请运行 clang-format;
- `Tools/Scripts/` 内含地址库工具:`gen_se_map_from_history.py` 从 git 历史生成 AE→SE ID 映射表,`gen_ae_to_se_map.py` 解析/校验 Address Library 二进制(format 1/2/5);
- 提 PR 请指向 `dev` 分支。

## 🐛 反馈问题

请在仓库的 "Issues" 页面提交,附上可复现步骤、游戏版本、SKSE 版本与服务器日志,详细的报告对开发非常重要。

## 📁 主要源码结构

* [**Code/client/**](./Code/client):天际客户端(SkyrimTogether.dll 核心,逆向 SDK + 各同步服务);
* [**Code/skse_bootstrap/**](./Code/skse_bootstrap):零依赖 SKSE 引导插件(自部署运行时);
* [**Code/skse_client/**](./Code/skse_client):SKSE 启动路径的客户端 DLL 封装;
* [**Code/server/**](./Code/server) + [**Code/server_runner/**](./Code/server_runner):专用服务器实现与启动器(含 Win32 控制面板);
* [**Code/immersive_launcher/**](./Code/immersive_launcher):ST 独立启动器;
* [**Code/encoding/**](./Code/encoding):网络消息定义(客户端/服务器共享协议);
* [**Code/skyrim_ui/**](./Code/skyrim_ui):游戏内联机界面(Angular/TypeScript,CEF 渲染);
* [**Code/tp_process/**](./Code/tp_process):CEF 覆盖层工作进程;
* [**GameFiles/Skyrim/**](./GameFiles/Skyrim):随 mod 分发的数据文件(esp、脚本、地址库、ID 映射表、FOMOD 向导);
* [**Tools/Scripts/**](./Tools/Scripts):构建/逆向辅助脚本。

## 📄 许可证

[![GNU GPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

本项目基于 GPLv3 许可(继承自 Tilted Online / TiltedEvolution),可自由使用、修改与再分发,衍生作品须保持同一许可证。
