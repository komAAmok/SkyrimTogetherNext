# 更新日志

本项目的更新记录以 **GitHub Releases** 为准:

> https://github.com/komAAmok/SkyrimTogetherNext/releases

每个 tag(形如 `v1.0.20`)对应一个 Release,附上版本号相同的两个包——
客户端 mod 与专用服务器。逐条提交的历史见 `git log` 与各次 PR。

## 未发布(2026-09-26)

第四次审计,以及**一次真实缺陷的回滚**。

- **修复地址库文件误删**:上一个提交在搬移 `STRPluginMessagingAPI.ini` 时,
  连带删掉了 `GameFiles/Skyrim/SKSE/Plugins/` 下**全部 34 个**地址库文件
  (`version-*.bin` 10 个、`versionlib-*.bin` 13 个、`versionlib-ae-to-se-*.map` 11 个)。
  打包路径直接吃 `GameFiles/Skyrim/`,少了它们玩家一启动就会"地址库失败"并退出。
  **34 个文件已按删除前的 blob 哈希逐一还原**;
- **`#if 0` 逐块判读**:22 处 → **删 15 留 7**。留下的 7 处都是"改 0 为 1"型开关
  (F6/F7/F8、旧战斗瞄准 ×3、地图菜单、imgui 上游 ×2),判据是**块外有没有为它留位置**;
- **debug 天气开关从未生效**:`Sky` 三个 hook 里的 `s_shouldUpdateWeather` 判据
  全在 `#if 0` 里,开关写了个没人读的变量。判据已启用(该变量只由 debug 窗口写,
  而该窗口在 release 版被裁掉,所以对发布版无影响);
- **EF 哨兵结论写进代码**:查清 SKSE 插件路径**本来就不需要** `_initterm_e` 哨兵
  (SKSE 已先加载 EF,我们的 hook 装在其 thunk 之上),只补注释、不改逻辑;
- **发布冻结解除**:`RELEASE-FREEZE.md` 删除,README 中英横幅同步移除;
  `release.yml` 的 guard 保留(冻结文件仍是唯一开关)。**本次不打 tag。**

## v1.1.1(2026-09-25)

第三轮审计。本轮换了两条新判据(容器迭代中改动、链式解引用),并复核了
前两轮修改本身。

- **链式解引用**:`VisitInteriorCell` 里的
  `PlayerCharacter::Get()->GetParentCellEx()->formID` 在 **load 全程**会取到空
  cell(`VisitCell()` 只在入口判过玩家存在,进内层就不再判);`VisitExteriorCell`
  的坐标回退也可能为空。两处都已补；
- **`Actor::Create`** 取玩家后**连续解引用三次**,已改为取到即判、失败即返回；
- **`DebugService`** 对 `Actor::Create` 的返回值从未判空就连续解引用,
  且 `PlayerCharacter::Get()` 与 `baseForm` 同样未判——一并补上；
- **修正上一轮自己引入的缺陷**:`Actor::Create` 的玩家判空曾被插在
  `New()` **之后**,提前返回会泄漏刚分配的 actor。已改为**先判空、再分配**；
- 容器迭代类判据(10 处)逐个核对后确认**全部安全**:代码库统一采用
  "先收集、后改动"或"循环外 clear"。

## v1.1.0(2026-09-25)

性能与同步的算法级改进,以及两轮全仓库审计。

- **插值改 Catmull-Rom**:远端玩家移动由线性改为三次曲线,线段交界处方向连续;
  丢包时不再"冻结—跳变",改为沿最后速度做**有界外推**(上限 1.25),并对
  播放头落后窗口的情形补了下限 0,避免反向外推;
- **帧循环 16 ms → 8 ms**:`SetTimer` 按 15.625 ms 系统 tick 向上取整,
  16 ms 实为 31.25 ms(约 32 次/秒),8 ms 落在单个 tick 内(约 64 次/秒),
  远端玩家采样率翻倍;
- **移动更新复杂度**:`OnReferencesMoveRequest` 由 O(更新数 × 实体数)
  降为 O(实体数 + 更新数)(先建 serverId 索引再查表);
- **修复重复生成同一远程玩家**导致引擎在 `SkyrimSE.exe+0x23d000` 解引用空指针
  的崩溃;重复守卫补 `LocalComponent` 检查;
- **全仓库 `GetById` 解引用审计**(两轮,判据不同):修复 15 处无守卫解引用,
  含 `Actor::Create` 对玩家的连续三次解引用、`PlayerService`/`PartyService`
  对硬编码全局的写入(断线路径也写,退出时崩)、`VisitInteriorCell`
  在 loading 期间对空 cell 的链式解引用等;
- 热路径日志降级:启动首秒曾写 220 行,现降至 debug。

## 历史沿革

本项目 fork 自 [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution),
其 GitHub fork 链为 `tiltedphoques/TiltedEvolution` → `rfortier/TiltedEvolution-rwf`
→ 本仓库。

本文件此前保存的是**上游 2022 年 7 月之前**由工具自动生成的 changelog
(最后一条为 v1.38.3,2022-07-01),与本仓库使用的版本号体系(`1.0.x`)无关,
留在根目录容易让打包/报错的人对不上号,因此移除;需要那段历史可查上游仓库,
或本仓库的 git 历史(该文件在移除此内容前已被完整提交过)。
