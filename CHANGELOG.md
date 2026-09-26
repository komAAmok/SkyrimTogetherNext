# 更新日志

本项目的更新记录以 **GitHub Releases** 为准:

> https://github.com/komAAmok/SkyrimTogetherNext/releases

每个 tag(形如 `v1.0.20`)对应一个 Release,附上版本号相同的两个包——
客户端 mod 与专用服务器。逐条提交的历史见 `git log` 与各次 PR。

## v1.1.4(2026-09-26)

同步上游 **v1.8.2**(3 个提交,分叉点 `e0002073`),并按本仓库的多版本逻辑改写了两处
会在 1.5.x 上失效的写法。

### 上游同步

- **#899 按"原始 NPC + 房主选中的模板"重建等级化 NPC**:此前远端角色的 base 被**直接**
  换成房主选中的模板,丢掉了只存在于原始 NPC 上的数据(守卫的名字、默认装备等)。
  现在走引擎自己的 `CreateTemplateActorBase(原始, 模板)`,与单机解析结果一致;
  旧的临时 base 交给引擎的 `GarbageCollector` 回收(与 `RecalcLeveledActor` 同一套策略,
  但**不动**旧实现留下的静态 pick);
- **#893 排除各派系的监狱储物容器**:入狱时没收的赃物/随身物品容器按派系各存一份,
  同步它们等于让一个玩家的物品进另一个玩家的储物箱。现在遍历 `ModManager::factions`
  收集这些容器指针并从对象同步里排除(只比较指针,不读它们的内容);

### 1.5.x 兼容(本次同步的主要风险)

- **`GarbageCollector::Get()` 不再解引用零返回桩**:单例 id `400329` 在 1.5.97
  映射表里**不存在**,而 `POINTER_SKYRIMSE` 对未映射 id 返回的是"零返回桩"——
  照抄上游会变成**解引用地址 0**。已改为直查地址库,未映射返回 `nullptr`,
  调用点判空后保留旧 base(泄漏一个临时 form,不崩);
- **修掉一个上游引入的每帧 Disable/Enable 死循环**:上游用 `GetLeveledPick() == pPick`
  判断"重建完成",而这个值读的是 `SetLeveledCreature`(id `20231`,1.5.97 **未映射**)
  写进去的 extra data ⇒ 比较**永远为假**,`WaitingFor3D` 会每帧重新禁用再启用该角色。
  新增 `LeveledNpcSystem::CanRecordPick()`(只查一次并缓存),不可写时退回本仓库原有路径
  并把完成判定切回 `baseForm == pPick`;
- `TESObjectREFR::SetBaseForm` 更名为上游的 `SetObjectReference`(仅改名,布局不变);
- `TESFaction` 补 `CrimeData` / `ModManager::factions`:偏移不是猜的——
  form 数组自 `0x10` 起、每项 `0x18`、按 FormType 索引,Faction(11)→`0x118`
  与既有 Quest(77)→`0x748` 互相印证,三个 `static_assert` 同时钉住;

### 清单

- `Tools/missing_1_5_97_ids.txt`:新增两个未映射 id(`36460`、单例 `400329`),
  覆盖率 3066/3078(99.6%);
- `docs/PITFALLS.md` §3.1:记录本次同步的两处判断与查证手段。

## v1.1.2(2026-09-26)

第四轮审计:全仓复查 + 插件层专项,以及**一次真实缺陷的回滚**。

### 稳定性修复(全部为可复现的空指针)

- **服务端可被远程打崩**:同一连接上发第二次 `AuthenticationRequest` 时
  `PlayerManager::Create` 返回 `nullptr`,而调用点直接解引用。已改为拒绝重复认证;
- **`PlayerService` 三个每帧路径**在载入/主菜单(此时没有玩家)解引用空玩家指针:
  `RunRespawnUpdates`、`RunBeastFormDetection`、`RunDifficultyUpdates`;同文件另两处
  本来就判空,这三处漏了;
- **`DiscoveryService::DetectGridCellChange`**:`GetParentCellEx()` 与坐标回退
  **都**可能为空,而下一行直接读 `pCell->formID`。同文件另外两处对同一个调用都判了空;
- **`CalculateHealthPercentage`**:每帧传入 `PlayerCharacter::Get()`,函数内部不判空;
- **`WeatherService`** 三处 `Sky::Get()->` 未判空(其中一处由服务端消息驱动),
  同文件另外两处判了空;
- **`OverlayService`/`PartyService`/`InputService` 五处 `GetOverlayApp()->`**:
  该指针在 `Create()` 之前为空,而组队事件正是连接后立刻会到的;
- **`baseForm` 空指针**:仓库自己的调试视图里就写着 `if (!pRefr->baseForm)`,
  但 12 处日志/分支直接解引用它(含每帧的裸体检查与网络驱动的物体/魔法/角色路径);
- **`EntitiesView` 一个死守卫**:先写 `"UNNAMED"` 又在下一行无条件 `sprintf_s` 覆盖它,
  等于没判。

### 插件层(专项)

- **ProxyResolver 的映射监听器从来没被调用过**:`OStimTogether` 与 `IEDSyncTogether`
  都注册了 `registerListener`,而框架只把回调存起来、从不触发;OStim 的反向映射
  (`_connectionByProxy`)因此永远是空的,而它的注释写着这条映射是 "Required"。
  现在由 `on_construct/on_destroy<PlayerComponent>` 触发 `kAdded`/`kRemoved`
  (`FormIdComponent` 每实体只写一次、拆除时移除,所以这两类事件就是全部迁移);
- **`setLogCallback` 存了不用**:插件装上的回调永远不会被调用。现在框架自己的
  插件层诊断(丢弃超长 channel / 超限载荷)会通过它发出,且**在锁外**调用;
- **`setLocalDisplayName` 存了不用**:如实注明这是**有意不上线**的——
  发送者显示名只能来自服务端认证过的登录名,否则任何插件都能冒充别人;
- **服务端限流桶泄漏**:`m_buckets` 以单调递增、永不复用的 `PlayerId` 为键且从不清理,
  长开的服务器会一直涨。现在随玩家行一起移除。

### 快捷键:只保留 F2

按需求把**除 F2 以外的所有游戏内快捷键**注释或禁用:

| 键 | 原用途 | 处理 |
|---|---|---|
| **F2** | 联机菜单 | **保留**(唯一) |
| 右 Ctrl | F2 的别名 | **移除**(第二个绑定=第二个要维护的东西) |
| F3 | 调试菜单栏 | `#if 0`;`toggleDebugUI` 绑定仍在,变成纯 opt-in |
| F4 | 揭示其他玩家 | 移除按键;`reveal players` 按钮照常工作 |
| F6 | Discord 覆盖层解锁 | 置于 `if (false && …)`,并在注释里写明重新启用覆盖层时不得复活 |
| F7/F8 | 开发快捷方式 | 保持 `#if 0` |

### 版本支持(1.5.x / 1.6.x / 1.7.x)

- 复核 `VersionDb` 三条加载路径:format 1(1.5.x,SE id 经 ae-to-se 映射表翻译)、
  format 2(1.6.x AE)、format 5(1.7.99+ 密集偏移数组);
- **34 个地址库文件还原**(见上),1.5.x 的 10 个 `.bin` + 10 张映射表、
  1.6.x/1.7.x 的 13 个 `versionlib` 全部在位;
- `GamePatch::At` 的按版本偏移与 `legacyMeasuredOn` 前缀校验保持原样(1.5.97 之外的
  1.5.x 不会被套用 1.5.97 的偏移)。

### 上一轮遗留

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
  `release.yml` 的 guard 保留(冻结文件仍是唯一开关)。

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
