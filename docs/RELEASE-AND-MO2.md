# 打包发布与 MO2 安装说明

## 一、打包成 GitHub Release(自动构建)

本仓库自带 CI 构建,**不需要本地 Windows 编译环境**:

- 推送到 `main` / `dev` 分支会自动触发
  `Playable Skyrim Together Build` 工作流,产物在 Actions 页面的
  Artifacts 里下载(名为 "Skyrim Together Build (版本号)")。
- **打 tag 自动发布 Release**(由 `release.yml` 完成):

  ```
  git tag v1.0.0
  git push origin v1.0.0
  ```

  Actions 会在 GitHub 的 Windows 云端机器上构建并发布 Release,包含三个包:

  | 文件 | 内容 |
  |---|---|
  | `*-full.zip` | 完整安装包:`SkyrimTogetherReborn/`(全部二进制:客户端 dll、启动器、服务器、UI)+ GameFiles(esp、脚本、全部地址库 `.bin`、AE→SE 映射 `.map`) |
  | `*-mo2-data.zip` | **MO2 专用 Data 包**:只有 esp、脚本、地址库、映射表,可直接用 MO2"从文件安装" |
  | `*-symbols.zip` | 调试符号(pdb),崩溃报告时有用 |

  也可以在 Actions 页面手动 `Run workflow`(workflow_dispatch),此时产物
  作为 Artifact 提供而不发布 Release。注意:仓库里至少要有一个 tag,
  否则版本号生成步骤会失败。

  在自己的 fork 上该工作流同样可用(仅每周定时构建被禁用,手动/推送触发不受影响)。

## 二、完整安装(不用 MO2)

1. 解压 `*-full.zip`。
2. 把 `SkyrimTogetherReborn/` 里的**全部文件**复制到游戏根目录
   (与 `SkyrimSE.exe` 同级)。
3. 把 zip 里其余内容(`SkyrimTogether.esp`、`scripts/`、`meshes/`、
   `SKSE/` 等)复制到游戏的 `Data/` 目录。
4. 装好对应版本的 SKSE(见 [LAN-RADMIN-GUIDE.md](LAN-RADMIN-GUIDE.md) 的版本表)。
5. 通过 `SkyrimTogether.exe` 启动游戏;房主另开 `SkyrimTogetherServer.exe`。

## 三、MO2 安装方式

Skyrim Together 支持两种从 MO2 启动的方式:

### 方式 A:SKSE 启动(推荐,MO2 常规流程)

客户端自带 SKSE 插件 `SkyrimTogetherSKSE.dll`(位于 `Data/SKSE/Plugins/`,
full 包和 mo2-data 包均已附带)。装好后,**通过 MO2 正常启动
`skse64_loader.exe` 即可自动加载 Skyrim Together**,无需运行
`SkyrimTogether.exe`:

- **前置条件(一次性)**:先按"完整安装"把 `SkyrimTogetherReborn/` 里的
  运行时文件复制到游戏根目录——插件加载时依赖其中的
  `libcef.dll` 等 DLL(SKSE 只扫描 `Data/SKSE/Plugins/` 找插件,但插件的
  依赖 DLL 要从游戏根目录解析,MO2 不会虚拟化游戏根目录);
- 兼容 SKSE 2.0.20(游戏 1.5.97)到 SKSE 2.2.x(游戏 1.6.1170)/新版
  1.7.x SKSE;
- 1.5.97 需要的地址库 `version-1-5-97-0.bin` 和映射表
  `versionlib-ae-to-se-1-5-97-0.map` 已随包附带(在 Data 包里,走 MO2
  虚拟文件系统);
- 若同时安装了其他 SKSE 插件,ST 与它们共存加载。

### 方式 B:ST 启动器启动

1. **手动部分(一次性)**:解压 `*-full.zip`,把 `SkyrimTogetherReborn/`
   里的文件复制到游戏根目录。
2. **MO2 部分**:下载 `*-mo2-data.zip`,在 MO2 中
   "安装新模组(从文件)" 直接安装它,并启用。
   包内含 `metadata.ini`,MO2 可直接识别;esp、脚本、地址库、映射表
   都会通过 MO2 的虚拟文件系统生效。
3. **从 MO2 启动**:在 MO2 的"执行程序"里新增一个条目,指向游戏根目录的
   `SkyrimTogether.exe`。项目的启动器已内置 usvfs 检测,在 MO2 下正常工作。

服务器 `SkyrimTogetherServer.exe` 与 MO2 无关,房主在任意位置运行即可。

> 注意:方式 B 中如果**不经过 MO2** 直接双击 `SkyrimTogether.exe`,MO2
> 虚拟的文件不可见,此时地址库和 esp 必须真实存在于游戏 `Data/` 中
> (即"完整安装"方式)。方式 A 则完全依赖 MO2 的虚拟文件系统,无需手动
> 复制任何数据文件。

## 四、给联机伙伴的最低要求

- 所有人使用**同一个 Release 构建的客户端**(服务器会校验构建号)。
- 游戏版本可以不同(服务器不校验),但建议一致。
- 地址库文件已随包附带;若单独分发,需从
  [Nexus 32444](https://www.nexusmods.com/skyrimspecialedition/mods/32444) 获取。
