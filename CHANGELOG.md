# Another Key for Shout (AKFS)

目标运行时：Skyrim SE 1.6.1170 (AE) · CommonLibSSE-NG + Address Library

- **发布版本号**：1.0.2（三位结构 major.minor.patch，见 `src/Version.cpp`）
- **内部迭代号**：v1.5

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