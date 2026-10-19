# Knowledge/SHOUT_CAST_MECHANISM.md — 原生龙吼施放的唯一可靠原语 (定稿, 依据底稿)

> 针对"让 X 键第二槽走引擎**原生 Shout 通道**(吼声+屏幕抖动+分级+原版冷却)"的最终定稿。
> 结论基于 CommonLibSSE-NG 3.6.0 头文件逐行核对(见下方标注) + MEMORY/Knowledge 历史实测记录。

## 一、核心结论 (直接回答任务三问)

### A) 原生吼声+抖动的**唯二**引擎通道

1. **`Actor::AddCastPower(SpellItem*)`** — **Z 键原版施放入口**。
   - 走引擎 **Shout 通道** → 原生吼叫音效 + 屏幕抖动 + 相机震动(这正是 `CastSpellImmediate`
     缺失的部分)。
   - 施放的**正是传入的那个 spell**，与当前 Z 槽绑定项无关(见 B)。
   - 签名与地址：`RE/A/Actor.h` line 499 → `void Actor::AddCastPower(SpellItem* a_power);`
     底 = `RELOCATION_ID(37787, 38736)`(AE 1.6.1170 正确 id)。`Actor.cpp` line 75-80 已实现。
2. **引擎 Z 键原生蓄力** — 完整蓄力→分级→吼声→冷却。但**必须真按 Z**(或引擎可接受的
   物理 Z 键事件流，见下——已证 AE 不可靠)。

### B) "施放解析到 Z 绑定龙吼"的唯一成因

`AddCastPower(spell)` 施放**参数里的 spell**，与 `high->currentShout`/词槽无关。
失败 #2("换成第二槽后还是 Z 龙吼")**不是 AddCastPower 的锅**——那是 v0.8 换用
**原生 Z 注入**(换 `high->currentShout` + 合成 Z 键 ButtonEvent 喂 ShoutHandler)才重现的。
按 MEMORY §四之四("井注 Z 键通道在 AE 不可靠")，**该通道已弃用**。

### C) 两类角色的答案

- **龙吼(TESShout)**：`player->AddCastPower(GetShoutVariationSpell(shout, level))` —— 挑好等级后
  取 `variations[level].spell` 传给 AddCastPower。**引擎原版蓄力/吼声/冷却/相机抖动全到。**
  ⚠️ **不加分等级 DLC / 不做原版蓄力选择**——AddCastPower 是"直接按该级施放"，无 Charge 状态机，
  无法完美还原"按住选级"的蓄力表现(见 §三)。若必须原生蓄力，只能走"真 Z 键"。
- **力量(TESForm 内 kPower/kLesserPower)**：非龙吼无吼声需求，`CastSpellImmediate` 足够；
  v0.5 起已实践。

## 二、源代码依据 (CommonLibSSE-NG 3.6.0, 逐行)

- <@"E:\SE mod\thirdparty\CommonLibSSE-NG\include\RE\A\Actor.h:499  void Actor::AddCastPower(SpellItem* a_power);  // Z 键原版施放入口"@>
- <@"E:\SE mod\thirdparty\CommonLibSSE-NG\src\RE\A\Actor.cpp:75  void Actor::AddCastPower(...)  { RELOCATION_ID(37787, 38736) }  // AE 地址 Library"@>
- <@"E:\SE mod\thirdparty\CommonLibSSE-NG\include\RE\T\TESShout.h:34  struct Variation { TESWordOfPower* word; SpellItem* spell; float recoveryTime; }"@>
- <@"E:\SE mod\thirdparty\CommonLibSSE-NG\include\RE\T\TESShout.h:69  Variation variations[3]"@>

## 三、诚实边界 (不可实现的限制)

- **"长按 Z 逐级蓄力 + 期间任意时刻松开出对应级 + 吼声 + 抖动"作为一个整体**，在 AE 上
  除"真按 Z"外**不可通过瞬发原语还原**。`AddCastPower` 走 Shout 通道拿到吼声/抖动，但它是
  **瞬发**，没有按蓄力时长选级的中间状态机。
- 若用户坚持"完全原版蓄力手感"(按住→升阶音效→松开按级吼)：**唯一可靠做法是把第二槽龙吼
  真正绑到 Z 槽**(写成 `currentShout` 但引擎据此施放的是"装备 Z 槽"，即你得同时保证该龙吼在
  Z 槽) **并触发引擎真 Z 键**。这正是 MEMORY §2.1 已锁定的"谁放谁解除、与原版机制相同"的
  方向，但 v0.8 注入合成的 Z 按键在 AE 实测不可靠(§四之四) → **结论：不注入，改绑槽真实换**。

## 四、推荐实现 (已按头文件核对)

```cpp
// Shout 分支：FetchShoutCastLevel() 自算等级(瞬时点按=1级)后——
RE::SpellItem* spell = shout->variations[level].spell;   // TESShout::Variation::spell
if (player) {
    player->AddCastPower(spell);                          // 原生吼声 + 抖动 + Shout 通道
    // 可选：自算冷却写回 SetVoiceCooldown(variations[level].recoveryTime)
}
// Power 分支不变：CastSpellImmediate(kInstant)。
```

> **决定**：v0.8 的"原生 Z 蓄力注入"整体回退为 **`AddCastPower` 施放**(Access 上唯一的
> 原生吼声+抖动原语)，力量仍 `CastSpellImmediate`。这与 MEMORY §四之七(2026-10-13 终稿)
> 的既定方向一致；若用户仍需"长按蓄力选级"，则只能走"第二槽→与 Z 槽真实换绑"的原版通道
> (不在本文件讨论的瞬发原语范围内)，或接受"点按即 1 级吼+吼声"的取舍。
