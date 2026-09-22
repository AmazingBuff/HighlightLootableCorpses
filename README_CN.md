# Highlight Lootable Corpses（高亮可搜刮尸体）

[English](README.md) | [简体中文](README_CN.md)

一款面向 **Skyrim Special Edition / Anniversary Edition（天际特别版/周年版）** 的尸体检测覆盖层（SKSE 插件）。它会高亮你身边所有**仍可搜刮的尸体**——死亡的 NPC 与生物、复活动物留下的灰堆、静态尸体容器——让你再也不会因为高草、灌木、岩石或地形遮挡而找不到战利品。尸体一旦没有可拾取的物品，就会立刻从覆盖层中消失。

## 功能特性

- **只标记可搜刮的尸体**——物品栏中仍有可拾取物品的尸体才会被高亮；搜空的尸体会从覆盖层中消失
- **透视一切遮挡**——高亮绘制在场景渲染之后，无视场景深度，草丛、灌木、墙壁和山坡都无法遮住尸体
- **三种显示模式**——`silhouette`（剪影：逐像素填充最近的高亮表面）、`outline`（描边：明亮内核加向外的彩色光晕）、`icon`（图标：尸体包围盒上方的向下箭头；距离较近的密集目标共用一个更大的双箭头）
- **精准贴合**——位置来自布娃娃物理体与 Havok 碰撞包围盒，肢解后按最大集群处理，并有几何体兜底
- **灰堆支持**——复活/分解敌人留下的灰堆（含 Dawnguard / Dragonborn 的灵魂余烬、灰烬魔人等变体）会追溯到原角色进行检测
- **静态尸体支持**——矮人伏击尸体/包裹尸体、烧焦尸体、幽灵尸体、蛛网裹尸、冰冻猛犸等容器型尸体，含 DLC 变体
- **可选战利品过滤**——只显示物品栏中包含以下物品的尸体：任务物品、钥匙、附魔装备、高价值物品（价值不低于 `HighValueThreshold` 金币）、书籍（法术/技能/未读标志）、消耗品（箭矢、食材、药水、卷轴、灵魂石）
- **已搜刮尸体隐藏**——可选择不再高亮你至少搜刮过一次的尸体，即使一无所获；标记按存档持久保存在 SKSE co-save 中
- **热键**——用一个按键开关覆盖层，或触发一次定时脉冲高亮（默认未绑定；通过 INI 的 `Hotkey` 键重新绑定）
- **游戏内设置面板**——所有选项都能在 Mod Control Panel（模组控制面板）中实时调整（需要 [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)）；修改立即生效
- **QuickLoot IE 支持**——安装了 [QuickLoot IE](https://www.nexusmods.com/skyrimspecialedition/mods/120075) 分支版时，打开它的拾取菜单等同于搜刮该尸体（可选）
- **轻量**——节流的后台扫描加上极低的逐帧开销；几乎不影响帧时间

## 环境要求

- [Skyrim Special Edition / AE](https://store.steampowered.com/app/489830/) **1.6.629 或更新**（已在 1.6.1170 和 1.7.99 上验证）。SE 1.5.97 与 Skyrim VR 理论上支持。
- 与游戏版本匹配的 [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)（All in one 版）
- 可选：[SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352)——启用游戏内设置面板。没有它模组也能正常工作，通过 INI 文件配置即可。
- 可选：[QuickLoot IE](https://www.nexusmods.com/skyrimspecialedition/mods/120075)——打开它的拾取菜单算作搜刮该尸体，用于"隐藏已搜刮"功能。

## 安装

1. 安装 SKSE64 和 Address Library（链接见上）。
2. 用 MOD 管理器（MO2 / Vortex）安装本模组，或手动把 `HighlightLootableCorpses.dll` 复制到 `Data\SKSE\Plugins\`。
3. 启动游戏。首次运行时会在 `Data\SKSE\Plugins\HighlightLootableCorpses.ini` 自动生成默认配置文件。

日志文件（用于排错）：

```text
Documents\My Games\Skyrim Special Edition\SKSE\HighlightLootableCorpses.log
```

## 使用方法

- 默认覆盖层是**关闭**的（`Enabled=false`），也没有绑定热键（`Hotkey=0`）。在设置面板中开启，或在 INI 中设置 `Enabled=true` / `Hotkey` 按键代码。
- 启用后，插件会按 `ScanIntervalMs`（默认 100 毫秒）扫描，并高亮 `MaxDistance`（默认 500 游戏单位）内的所有可搜刮尸体。
- 想用按键开关或脉冲覆盖层，把 INI 中的 `Hotkey` 设为一个虚拟键码（0 = 未绑定）：
  - `HotkeyMode=0`（constant，常亮模式）：按键切换覆盖层开/关。
  - `HotkeyMode=1`（pulse，脉冲模式）：按键后未搜刮的尸体高亮 `PulseDurationMs` 时长，随后淡出。
- 拿走尸体上的所有物品后，它的高亮即消失——灰堆和静态尸体容器同样如此。
- 可选择隐藏至少搜刮过一次的尸体——即使一无所获，它们也会在跨存档时保持隐藏：标记按存档持久保存在 SKSE co-save 中并被自动清理（INI 中的 `HideSearchedEnabled`，或面板中的 "Hide Searched Corpses" 复选框）。支持原版互动，也支持 QuickLoot IE。
- 打开 **Mod Control Panel → Highlight Lootable Corpses → Settings** 实时调整所有选项（颜色、距离、淡出、战利品过滤、扫描间隔）。修改立即生效；点击 *Save* 按钮写入 INI。面板打开时热键被挂起。

## 配置

所有选项都在 `Data\SKSE\Plugins\HighlightLootableCorpses.ini` 中。每个选项也可以在游戏内面板中调整。每次保存游戏时 INI 会自动重写，面板的 *Save* 按钮也可随时写入。下表为首次运行默认值。

### [General]

| 键 | 默认值 | 范围 | 说明 |
| --- | --- | --- | --- |
| `Enabled` | `false` | — | 模组是否启用。 |
| `Hotkey` | `0` | 0–0xFE | 热键虚拟键码；0 = 未绑定。 |
| `HotkeyMode` | `0` | 0–1 | 0 = constant（开关切换），1 = pulse（定时高亮）。 |
| `PulseDurationMs` | `500` | 500–30000 | 脉冲模式：高亮持续多少毫秒后完全淡出。 |
| `ScanIntervalMs` | `100` | 100–1000 | 尸体扫描间隔（毫秒）。 |

### [Display]

| 键 | 默认值 | 范围 | 说明 |
| --- | --- | --- | --- |
| `DisplayMode` | `0` | 0–2 | 0 = silhouette（填充剪影），1 = outline（明亮内核加光晕），2 = icon（尸体上方的距离缩放箭头；密集目标共用双箭头）。 |
| `OutlineThickness` | `1` | 1–20 | 描边光晕大小；数值越大亮边与外层光晕越宽。 |
| `IconRadius` | `5` | 5–20 | 图标的基础半宽（像素）。 |
| `OutlineColor` | `00FF66` | ARGB 十六进制 | 高亮颜色。 |
| `MinOpacity` | `0.0` | 0–1 | 最大距离处的最低不透明度。 |
| `MaxDistance` | `500` | 500–5000 | 搜索半径（游戏单位，默认约 7 米；70 单位 ≈ 1 米）。 |
| `FadeStartDistance` | `0` | 0–MaxDistance | 开始淡出的距离（低于该值完全不透明）。 |
| `FadePower` | `0.1` | 0.1–4 | 淡出曲线指数（越大淡出越快）。 |

超出范围的值在加载时会被钳制：非法的 `DisplayMode`（> 2）变为 outline，非法的 `HotkeyMode`（> 1）变为 constant，超范围的 `Hotkey`（> 0xFE）变为未绑定。

### [LootFilter]

| 键 | 默认值 | 范围 | 说明 |
| --- | --- | --- | --- |
| `HideSearchedEnabled` | `false` | — | 玩家搜刮（激活）过至少一次的尸体不再高亮，即使一无所获。 |
| `ValueFilterEnabled` | `false` | — | 只高亮符合下列类别的尸体。 |
| `ValueQuestItems` | `false` | — | 任务物品。 |
| `ValueKeys` | `false` | — | 钥匙。 |
| `ValueEnchanted` | `false` | — | 附魔装备。 |
| `ValueHighValue` | `false` | — | 任何单件物品价值 >= `HighValueThreshold` 金币。 |
| `HighValueThreshold` | `0` | 0–500 | 高价值阈值（金币）（金堆按数量计）。 |
| `BookFilterMode` | `0` | 0–0xFF | 位标志：1 = 法术书，2 = 技能书，4 = 未读书籍，7 = 全部。 |
| `ValueConsumables` | `false` | — | 箭矢、食材、药水、卷轴、灵魂石。 |

图标距离缩放：图标由近处的 1.25 倍平滑缩小到远处的 0.75 倍（smoothstep）；聚集的尸体共用一个放大 1.2 倍的双箭头，整体上限 1.5 倍。

## 性能

- 扫描以任务形式在游戏主线程上按 `ScanIntervalMs` 运行，且**只做检测**（战利品过滤、包围盒、距离）——不再为几何体遍历场景图。
- 遮罩几何体在渲染线程上收集，位于以 form id 为键的 LRU 缓存之后（容量 32，与尸体的数量上限一致）：尸体首次出现在视野内的那一帧立即就地收集其几何体，因此快速转动视角后，新进入视野的尸体会立刻高亮，而不用等待最多一个扫描间隔；稳定状态下各帧直接复用缓存几何体，不再遍历场景图。缓存条目被淘汰时（最近使用优先跟踪的是被绘制的尸体）其几何体才会重新收集。
- 逐帧开销极小：渲染阶段按视锥剔除屏幕外目标、刷新颜色，并从缓存整理绘制列表。
- 每具尸体的工作量有上限：最多跟踪 32 具尸体，每帧最多绘制 256 个渲染几何体。
- 各模式的相对开销：**icon < silhouette < outline**——图标模式最省，剪影模式居中，描边模式开销最大（需为每个目标计算光晕）。

## 兼容性

- 战利品检测读取的与容器界面相同的合并物品栏（基础容器 + 运行时变更），因此尸体会恰好在其仍有可拾取物品时被高亮。脚本添加与玩家丢入尸体的物品也算战利品，与原版一致。
- 不修改任何游戏数据记录；覆盖层是纯渲染。在已有存档上安全添加或移除。
- 已搜刮标记保存在 SKSE co-save 中，并以插件自己的记录 ID 为键；读档时会自动清除过期的 form id。添加或移除本模组不会损坏存档。
- 应与任何拾取类或 ESP 类模组兼容；加载顺序无关紧要。

## 故障排查

- **什么都不显示**——覆盖层默认是关闭的（`Enabled=false`）；先在面板或 INI 中启用。再确认尸体确实在 `MaxDistance` 范围内、仍有战利品，且已安装 Address Library。
- **没有游戏内面板**——未安装 SKSE Menu Framework；模组其余功能不受影响。通过 INI 文件 `Data\SKSE\Plugins\HighlightLootableCorpses.ini` 配置即可。
- **HDR 显示器上颜色异常**——游戏 HDR 后备缓冲只有 2 位 alpha，半透明高亮会被量化；调低 `MinOpacity` 可以让效果更柔和。
- 查看日志 `Documents\My Games\Skyrim Special Edition\SKSE\HighlightLootableCorpses.log`——其中记录了检测判定与安装问题。

## 从源码构建

环境要求：Visual Studio 2022、CMake 和 [vcpkg](https://vcpkg.io/)（需设置 `VCPKG_ROOT` 环境变量）。CommonLibSSE-NG 以 git submodule 方式使用（`extern/CommonLibSSE`），请先初始化子模块。

```text
git submodule update --init --recursive
cmake --preset Release
cmake --build build --config Release
```

插件 DLL 输出到：

```text
build/Release/HighlightLootableCorpses.dll
```

构建目标为 Skyrim SE 与 AE（VR 支持未编译）。依赖项（经 vcpkg）：fmt、spdlog、SimpleIni、DirectXMath、DirectXTK。

## 许可证

基于 [GPL-3.0](LICENSE) 许可证分发。

## 致谢

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) — alandtse 及贡献者
- [SKSE](https://skse.silverlock.org/) — SKSE 团队
- [SKSE Menu Framework](https://github.com/QTR-Modding/SKSE-Menu-Framework-3) 与 [SKSE-MCP](https://github.com/QTR-Modding/SKSE-MCP) — QTR-Modding
