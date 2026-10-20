# Knowledge/DATA.md — 项目结构/实体知识 (脚手架映射)

> 本文件归入 pmgov 脚手架 `Knowledge/` 目录，作为结构/实体知识来源。权威清单见根 `DATA.md`（完整 125 行）。此处为检索用摘要。

## 目录结构 (目标 Layout, ✅=已落地)

```
E:\SE mod\
├── MEMORY.md        ✅ 记忆
├── CLAUDE.md        ✅ 编码规约
├── DATA.md          ✅ 结构/实体
├── CMakeLists.txt   ✅ 顶层 CMake
├── thirdparty\      ✅ spdlog / CommonLibSSE-NG / rapidcsv
├── scripts\         ✅ build_spdlog.ps1
├── src\             ✅ main.cpp / Version.cpp / SecondSlotManager.{h,cpp} / MenuMouseIntercept.{h,cpp}
└── build\out\Release\ ✅ AKFS.dll
```

## 代码实体

- **`SecondSlotManager`** (services)：X 键第二槽管理器。监听 X 键(BSTEventSink<InputEvent*>)；按 `UI::IsMenuOpen(MagicMenu)` 分化——菜单开=`StoreHighlightedToSlot()`、菜单关=`CastSecondSlot()`(经 **`GetMagicCaster(kInstant)->CastSpellImmediate`** 立即施放)。X 键判定用**物理扫描码** `GetIDCode()==REX::W32::DIK_X`(不靠 QUserEvent，实测空命令名失效)。持有 `m_secondSlot`(TESForm*)。**已实测可用**。
- **`MenuMouseIntercept`** (工具)：纯查询无 hook —— `IsZCastForm`(Shout 恒真；Spell 的 kPower/kLesserPower/kVoicePower)、`GetShoutFromForm`/`GetSpellFromForm`(龙吼取 variations[0].spell)、`ResolveHighlightedForm`(读魔法菜单高亮项 formID→TESForm。实测路径: `Menu_mc.inventoryLists.panelContainer.itemList` → `itemList.selectedIndex` → `entryList[选中]` → **entry.formId**(小写d/number，无 data 层))。

## 游戏实体/API (CommonLib 关键)

- `Actor::AddCastPower(SpellItem*)` — RELOCATION_ID(37787, 38736)，Z 键施放函数。
- `Actor::ACTOR_RUNTIME_DATA::selectedPower`(TESForm*, 0x1E0) 力量槽；`HighProcessData::currentShout`(TESShout*, 0x008) 龙吼槽；`magicCasters[4]`(0x1A0)。
- `TESShout::variations[3]`(0x60，各含 `SpellItem*`)，施放龙吼取 `variations[0].spell`。
- `ActorMagicCaster::SetCurrentSpellImpl` 为空实现 → 施放不依赖常驻 currentSpell。
- `VTABLE_MagicMenu[0]` aeID=215652（AE/1.6.1170 取此），vtable 槽位 4 = ProcessMessage。