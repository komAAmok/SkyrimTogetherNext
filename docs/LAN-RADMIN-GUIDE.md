# 局域网联机指南(Radmin LAN / Hamachi / 物理局域网)

Skyrim Together 的网络层使用 **GameNetworkingSockets(纯 UDP/IP)**,不依赖
Steam 联机 API(无 Steam P2P、无 Steam 中继、无 Steam ID 认证)。因此通过
Radmin LAN、Hamachi 或同一物理局域网直连即可联机,**无需 Steam 在线**。

## 支持的游戏版本

地址库数据来自 Nexus Mods 模组
[Skyrim Address Library(模组 32444)](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
的 "All in One" 压缩包。把压缩包里的 `SKSE/Plugins/` 下所有 `.bin` 文件直接
解压到游戏目录的 `Data/SKSE/Plugins/` 即可(文件名无需修改):

| 游戏版本 | 地址库格式 | 所需额外映射文件 | SKSE |
|---|---|---|---|
| 1.6.317 / 318 / 323 / 342 / 353 / 629 / **640** / 659 / 1130 / 1170 / 1179 | format 2 | 无 | 各版本对应 SKSE 2.1.x / 2.2.x |
| 1.7.99 / 1.7.104 | format 5 | 无 | 对应新版 SKSE |
| 1.5.97(SE)⚠ 实验性 | format 1 | `versionlib-ae-to-se-1-5-97-0.map` | SKSE 2.0.20 |
| 1.5.3 / 16 / 23 / 39 / 50 / 53 / 62 / 73 / 80(旧 SE)⚠ 实验性 | format 1 | `versionlib-ae-to-se-1-5-<版本>-0.map` | 各版本对应 SKSE 2.0.x |

**1.5.x 版本说明(实验性)**:旧版地址库的 ID 体系与 AE 不同,客户端需要
"AE ID → 该版本偏移"的翻译表。本项目仓库已随
`GameFiles/Skyrim/SKSE/Plugins/` 提供全部 10 个 1.5.x 版本的映射表
(由 `Tools/Scripts/gen_se_map_from_history.py` 从本仓库 git 历史中的
1.5.97 原始偏移自动生成,覆盖代码中约 97% 的地址引用),随 mod 安装时
会一并复制到 `Data/SKSE/Plugins/`。

**务必知悉**:约 3% 的地址(均为 1.6.x 时代新增的同步功能:物品附加数据、
天气、字幕、任务阶段、装备细节等)在 1.5.x 上没有可用映射,客户端启动时
已将其降级为不同步(启动时会弹出实验性警告)。可以联机,但可能出现局部
数据不同步。完整体验请使用 1.6.x 及以上。

> **稳定性提醒**:代码中的结构体成员偏移按 1.6.x 适配,1.5.x / 1.7.x
> 上个别系统仍可能有差异,遇到崩溃请反馈。推荐使用 1.6.640 / 1.6.1170。

## 服务端设置(房主)

1. 启动 `ST Server`(或 Docker 镜像),默认监听 **UDP 10578** 端口
   (被占用时自动 +1,以服务器控制台输出为准,可用 `GameServer:uPort` 指定)。
2. 在防火墙/路由器中放行该 UDP 端口(Radmin LAN 内只需放行本机防火墙)。
3. 建议所有玩家使用**相同的游戏版本**联机:服务器只校验 mod 构建版本,
   不校验游戏 exe 版本,但不同游戏版本之间的内容差异可能导致同步异常。

## 客户端连接(其他玩家)

1. 安装 Radmin LAN 并加入同一网络。
2. 启动游戏,两种方式任选其一:
   - **SKSE 启动(推荐 MO2 用户)**:把 Data 包装入 MO2 后照常通过
     `skse64_loader.exe` 启动,Skyrim Together 会作为 SKSE 插件自动加载
     (兼容 1.5.97 的 SKSE 2.0.20 与 1.6.x/1.7.x 的 SKSE);
   - **ST 启动器**:通过游戏根目录的 `SkyrimTogether.exe` 启动。
3. 房主在 Radmin LAN 中查看自己的 Radmin IP(形如 `26.x.x.x`)。
4. 进入游戏后按 **F2 或 右Ctrl** 打开 Together 菜单,服务器地址填
   `<房主Radmin IP>:10578`,输入服务器密码(如有)后连接。

## 常见问题

- **连接超时**:确认房主服务器已启动、UDP 端口已放行、双方都在同一 Radmin
  网络中;尝试互相 ping 对方 Radmin IP。
- **版本不匹配被踢出**:客户端与服务器必须是同一份 mod 构建的产物
  (服务器校验的是 mod 构建号 `BUILD_COMMIT`,与游戏版本无关)。
- **不需要 Steam**:启动器在检测不到 Steam 时会自动跳过 Steam 相关加载
  (仅影响 Steam 覆盖层/成就,不影响联机)。
- **提示加载地址库失败**:确认 `Data/SKSE/Plugins/` 里有与你的游戏版本
  对应的 `versionlib-*.bin`(1.6.x/1.7.x)或 `version-*.bin`(1.5.x),
  1.5.x 还需要对应的 `versionlib-ae-to-se-*.map`。
