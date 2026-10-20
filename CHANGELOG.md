# Another Key for Shout (AKFS)

目标运行时：Skyrim SE 1.6.1170 (AE) · CommonLibSSE-NG + Address Library

- **发布版本号**：1.1.1（三位结构 major.minor.patch，见 `src/Version.cpp`）
- **内部迭代号**：v1.12

## 发布 1.1.1（内部 v1.12）— 2026-10-20

本次为**冷却与槽位还原的可靠性修复**，解决 1.1.0 及更早版本遗留的三个问题。

### ① X 键龙吼无冷却（"假冷却"根因）

- **症状**：按 X 施放龙吼后立刻再按 X 仍能放出；但游戏 UI 的冷却条/提示显示还在冷却中。
- **根因**：v1.5 引入的"残留自愈"把 `voiceRecoveryTime > 0` 且"插件空闲"一律判成引擎残留垃圾并清零放行。
  但**插件空闲恰好就是冷却正常进行的整个时段**（施放已结束、槽位已还原）→ 每段真冷却开场即被清掉。
  当时之所以判断为"垃圾"，是基于"引擎从不写该字段（实测恒 0.00s）"的观察，而实测日志证明引擎**确实会写**
  且数值会自然递减（18:24:32 写入 35.65s → 18:25:12 自动归零）。**"假冷却"这个概念本身是错误观察的产物。**
- **修复**：删除清零与全部"自愈/标记/阈值"逻辑，只保留一条只读判断——`voiceRecoveryTime > 0` 即拦截，
  **一个引擎字段都不写**。冷却时长完全由引擎按 `TESShout::variations[阶].recoveryTime` 计算，
  玩家的减冷却属性/附魔正常生效，不再与 HUD 冷却条争抢同一字段。

### ② 冷却中按 X 篡改 Z 键龙吼

- **症状**：龙吼冷却期间按 X，Z 键管理的龙吼会被改成第二槽（X）的龙吼，且不可恢复
  （进入龙吼菜单可见 Z 槽选中已变）。原版只有一个 Z 槽，此问题属本插件接入方式的固有风险。
- **根因**：`BeginNativeShout()` 的执行顺序是**先换槽、后喂 Z 键事件**（`currentShout`/`selectedPower`
  临时改成第二槽）。一旦引擎拒绝该次施放，换出去的槽**没有任何还原路径** → 永久污染。
  经实测排查，`ShoutHandler::CanProcess()` **不做冷却检查**（日志 8/8 返回 true，含冷却 16.19s/33.92s/27.25s
  的情形），因此"先问 CanProcess 再换槽"并不能挡住冷却场景。
- **修复**：把冷却拦截移到**换槽之前**（`OnCooldown()` 龙吼分支），冷却中直接不进入换槽流程。
  该分支只读不写，不影响冷却数值本身。同时**保留**了 `CanProcess` 预检与"CanProcess 是否改动槽位"的校验日志，
  作为额外的防御层。

### ③ 快速连按导致 Z 槽永久丢失（隐蔽）

- **症状**：极难复现。在上一次施放**尚未还原**时再次按 X，会把当时的第二槽（X）龙吼记录成"原始 Z 槽"，
  并覆盖掉上一次尚未归还的真实原值 → Z 槽选择永久丢失。
- **根因**：`TryDeferredRestore()` 有 300ms 保护窗口（为让引擎异步结算读完 `currentShout`），窗口内直接返回不做还原。
  因此快速连按（实测间隔 88~94ms）时，`m_previousShout = high->currentShout` 记录到的是**借来的第二槽值**。
  实测日志：`[defer-restore] 延迟还原...`（21:41:47.493）→ `[begin] 原 currentShout=0002F7BA → 换为第二槽=0002F7BA`（21:41:47.587，仅隔 94ms）。
- **修复**：为槽位还原引入**可靠传输协议**（借鉴 ARQ / 停止-等待思想）：
  - **停止-等待**：`m_restorePending` 为真时**拒绝**开始新一轮施放，保证记录原值那一刻槽位一定是干净的
    （从根上杜绝"把脏值记为原始 Z 槽"，也杜绝"覆盖未归还的原值"）；
  - **写后校验**：还原写入后立即回读，确认 `currentShout`/`selectedPower` 真的回到原值，否则标记未确认；
  - **有限重传**：未确认则下一帧重发，最多 8 次，超过则告警放弃（避免死循环）；
  - **超时降级**：沿用 3s 硬上限。
- **实测验证**：修复后同样间隔（88ms）的连按被正确拒绝，日志出现
  `[begin] 上一轮槽位还原尚未确认（pending），本次 X 被拒绝以避免把脏值记为原始 Z 槽`，Z 槽保持完好。

### 已知代价

- 快速连按时，第二次按 X 会被**忽略**（等待上一次收尾）。这是停止-等待的固有取舍，
  用来换取"Z 槽绝不丢失"。冷却中按 X 亦为静默无反应（无引擎提示音）。

### 产物

- `AKFS.dll` 653824 字节，导出 `SKSEPlugin_Load` / `SKSEPlugin_Query` / `SKSEPlugin_Version` 已验证。
- 发布包：`Another Key for Shout 1.1.1.zip`（英文默认）、`Another Key for Shout 1.1.1_CHS.zip`（中文）。
- 源码包：`AKFS_source_1.1.1.zip`。

## 发布 1.1.0（内部 v1.7）— 2026-10-19

### 新增：SKSE Menu Framework 配置菜单（可选依赖）

无需 SkyUI / MCM / Papyrus / ESP，直接以 C++ 接入 [SKSE Menu Framework](https://github.com/QTR-Modding/SKSE-Menu-Framework-3)。

- **设置页**
  - 触发键：滑块调 DIK 扫描码，或点 `Capture` 后按任意物理键直接绑定。
  - 特殊能力每日一次：勾选框。
  - `DailyBypass` / `BlockedPowers`：输入 8 位 hex → 添加 → 列表点选 → 删除。
  - 界面语言：中文 / English，切换后立即生效。
  - 重新加载 ini：手改 ini 后不必重启游戏。
- **状态页**（ini 做不到的运行时信息）
  - 第二槽：formID + 名称 + 类型（龙吼 / 能力）；Z 槽龙吼同样显示。
  - 龙吼 / 能力冷却：实时倒计时。
  - 诊断：`voiceState`、`charging`、`restorePending`、ini 路径。
  - 动作：清空第二槽 / 重新加载 ini / 测试施放。
- **可选前置**：未安装 SKSE Menu Framework 时菜单静默跳过，X 键第二槽一切照旧。
- **配置写入策略**：菜单改动**先写内存（立即生效）再写 ini（持久化）**，两者一次完成。

### 本地化

`Language/Chinese/AKFS.ini` 与 `Language/English/AKFS.ini` 各自新增 `[Strings]` 节，安装时按所选语言拷成 `AKFS.ini`。
翻译成其它语言只需复制一个 ini 并翻译 `[Strings]` 节，无需改代码。
（菜单中文字体需用户自行在 SKSE Menu Framework 的配置里启用，属该框架自身设置。）

### 已知事项

- 构建：源码位于 `archive/AKFS/`，依赖仍在工作区根 `thirdparty/`；构建目录 `E:\SE mod\build`。
- `AKFS.dll` 649216 字节，导出 `SKSEPlugin_Load` / `SKSEPlugin_Query` / `SKSEPlugin_Version` 已验证。

## 发布 1.0.2（内部 v1.5）— 2026-10-14

### ⑤ 用久了无法释放龙吼（voice 冷却残留自愈）
- **症状**：长时间使用后，某次之后按 X 完全无法释放龙吼（OnCooldown 之前静默拦截，无任何日志/bug 表现）。
- **根因**：X 注入施放的龙吼，引擎**从不主动写 `voiceRecoveryTime`**（日志实锤：每次施放前/后恒 0.00s）。
  `OnCooldown` 龙吼分支的 `remain>0` 判据**依赖一个引擎不会自己归零的残留值**——一旦某次异步施放
  让引擎残留 `voiceRecoveryTime>0` 且不归零，就永久锁死龙吼槽（累积性，故"用久了"才出现）。
- **修复**：`OnCooldown` 龙吼分支加**残留自愈**——当 `voiceRecoveryTime>0` 但插件当前不在施放龙吼中
  （`!m_shoutActive && !m_restorePending`）时，判定为引擎残留垃圾冷却 → 直接清 `high->voiceRecoveryTime`
  归零并放行；仅在插件真在施放中才拦截。只清 recoveryTime，**绝不写 voiceState**（铁律）。

## 发布 1.0.1（内部 v1.4）— 2026-10-14

本次解决 4 条问题：

### ① 设槽后首次按 X 直出三阶（ROOT CAUSE 锁定）
- **症状**：设置 X 槽后第一次按 X，龙吼直接蓄力到第三阶段（≈16.8s 墙钟）。
- **根因**：设槽的 X 抬起设了 `m_storePendingRelease=true`；该次抬起在 up 分支被
  `if(m_storePendingRelease && !m_shoutActive) break` 吞掉——但 down 分支已开始真实施放
  （`m_shoutActive=true`），up 因此守卫**永不调 FinishNativeShout** → `m_shoutActive` 泄漏
  → 下一次按 X 走 held 用上一次的墙钟累计 → 直出三阶。
- **修复**：up 分支守卫改 `if(m_storePendingRelease && !m_shoutActive)`（仅在**未**开始真实施放时
  才吞 up 存槽标志）；down 分支非菜单时先 `m_storePendingRelease=false` 再 `OnXDown()`。

### ② 特殊能力"一天一次"隔天仍被卡死（v1.3）
- **症状**：当日用过大能力后，隔了一天仍无法触发，X 对能力永久失效。
- **根因**：`PowerUsedToday` 返回"条目存在 `find!=end`"而不是"存储游戏日 == 当前游戏日"；
  跨日清理只在成功施放路径跑，一旦被拦条目永不清除。
- **修复**：`PowerUsedToday` 改为比较 `int64(存储日) == int64(当前 GetDaysPassed())`，
  游戏日一跨自动解禁。游戏日取 `RE::Calendar::GetDaysPassed()` 整数部分。

### ③ 加载更小天数存档不会误封（v1.3.1）
- 因 ② 已改为**值比较**（当前游戏日），加载更小游戏日存档、重开游戏等情况
  只可能"更宽松"（视为未用可再施放），**永不误封**。

### ④ 只对 kPower 套"当日一次"，可反复能力/例外组合开后门（v1.4）
- **背景**：原 v1.3 对所有能力套当日门禁 → 误杀可反复用的 `kLesserPower`
  （吸血鬼领主变身 / Night Eye / 黑市等）。UESP 明文：Greater Powers(kPower)=每天一次，
  Lesser Powers(kLesserPower)=可任意次；Beast Form+净化 Ring of Hircine 是"kPower 却可反复"的例外。
- **修复**：新增 `IsDailyPower()` = `{SpellType==kPower} && !DailyBypassContains(formID)`；
  `OnCooldown` 仅对 IsDailyPower 查 `PowerUsedToday`，`CastPowerNow` 仅对它落库。
  `kLesserPower(3)/kVoicePower(11)` 恒只走 3s 短冷却。
- **后门**：ini `[Settings] DailyBypass=<formID1>,<formID2>…`（8 位 hex，逗号/空白分隔）。

## 配置项（AKFS.ini）

```
[Settings]
KeyCode=45            ; 触发键 DIK 扫描码（默认 X）
PowerOncePerDay=1     ; 大能力(kPower)是否"每游戏日一次"（默认开），0=关闭只走 3s 短冷却
DailyBypass=          ; 豁免"当日一次"的 kPower formID 清单（逗号分隔 8 位 hex）
                      ;   例：想让狼人变身可反复 → DailyBypass=00092C48
[Lang]
Language=中文          ; 或 English
```

## 部署

- 发布 ZIP（`Another Key for Shout 1.0.1.zip`）：FOMOD 直接给 Mod 管理器安装，含 DLL + 语言 ini。
- 手动：把 `AKFS.dll` 复制到游戏 `Data\SKSE\Plugins\AKFS.dll`，`AKFS.ini` 同级放置。
- 需已安装 Address Library for SKSE Plugins（SE 1.6.x）对应 runtime 1.6.1170。
- 日志：`My Games\Skyrim Special Edition\SKSE\Another Key for Shout.log`。