# 兼容 MO2（usvfs）启动 SkyrimTogether

## 背景
- `SkyrimTogether.exe` 启动器自行映射 `SkyrimSE.exe` 并加载 `skse64_x_x_x.dll`（调用 `StartSKSE()`），SKSE 插件与地址库从 `Data\SKSE\Plugins` 读取。
- 从 MO2 启动时 usvfs 注入进程，`immersive_elf/main.cpp` 检测后使 `EarlyInstallSucceeded()` 返回 false，`Launcher.cpp:86` 直接 DIE 退出——这是唯一的硬性拦截点。
- 本仓库其余部分（FileMapping.cpp 的 usvfs 专用 hook、MO2 下 XAudio2 崩溃规避、MO2Active 上报）均已适配 MO2。

## 修改内容（2 个文件，改动很小）

### 1. `Code/immersive_elf/main.cpp`
- `EarlyInstallSucceeded()` 改为始终返回 true（它目前没有任何真正会失败的早期安装逻辑，`InstallEarlyHooks()` 是空函数；返回 false 纯粹是 MO2 拦截闸门）。
- 保留 usvfs 检测，新增导出函数 `WasUSVFSActive()`，供启动器做提示用。

### 2. `Code/immersive_launcher/Launcher.cpp`
- 将 `if (!EarlyInstallSucceeded()) DIE_NOW(...)` 替换为：调用 `WasUSVFSActive()`，若为真仅记录一条日志（"Running under MO2 usvfs..."），不再退出。

### 不改动但依赖的现有逻辑（已具备，仅验证）
- SKSE 加载：`client/ScriptExtender.cpp` 扫描游戏根目录 `skse64_x_x_x.dll`（usvfs 虚拟化的文件枚举对其透明，MO2 安装的 SKSE 也能被看到）。
- 地址库/数据文件读取 `Data\SKSE\Plugins`：usvfs 会把 MO2 的 Data 重定向进去，正常工作。

## 用户使用方式（修改后）
1. 把 `SkyrimTogether.exe`（构建产物）以 MO2 工具/可执行文件的方式添加进 MO2，通过 MO2 启动；
2. SKSE 与其他 mod 照常用 MO2 管理；ST 会自动加载匹配版本的 `skse64_x_x_x.dll` 并调用 `StartSKSE()`。

## 验证
- 代码为 Windows 专用（xmake + MSVC），本 Linux 环境无法编译验证；需要你在 Windows 上用 `Build.bat` 构建，并按上述方式在 MO2 里实测。我会保证改动语法正确、逻辑自洽。
- 若实测仍有 usvfs 相关崩溃，最可能的后续点在 `stubs/FileMapping.cpp`（该文件已有多处 MO2 专项处理，是继续排查的第一落点）。