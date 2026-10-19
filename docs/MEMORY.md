# MEMORY.md — 项目记忆

> 本文件(MEMORY.md)集中记录**跨会话、不断发展的事实记忆**：项目是什么、正在做什么、已经决定/否决了什么、待办、踩过的坑。每次工作完成后，把重要的新事实追加或更新到这里。
>
> 职责边界：
> - **编码规范/习惯** → 见 `CLAUDE.md`
> - **项目结构/实体清单** → 见 `DATA.md`
> - **这里只放「发生了什么/决定是什么」的记忆**，不放规则，不放结构清单。

---

## 1. 项目身份

- **项目类型**：上古卷轴 5：天际 特别版 (Skyrim SE / Anniversary Edition) 的 SKSE 插件 (Plugin / DLL Mod)。
- **目标 Runtime**：**Skyrim SE 1.6.1170**
- **技术路线**：**CommonLibSSE-NG** + **Address Library** 实现通用版本兼容。
  - CommonLibSSE-NG 提供一套 API，编出来的 DLL 可跨多个 runtime 版本运行，无需针对每个版本单独 PDB。
  - Address Library (SKSE 插件，使用 `vrm`/`REL` 地址数据库) 为 1.6.1170 及 n 个版本提供一致的 ID 地址解析。
- **构建工具链**：CMake + MSVC (x64)。依赖通过 vcpkg 或 FetchContent 引入 CommonLibSSE-NG。
- **草稿定位**：这是一个还处于空白起步阶段的项目 — 工作区 `E:\SE mod` 目前为空。

---

> **⚠️ 提示**：下方第 2 节的「第二槽底层选址」「X 键第二槽插件进度」描述的是**旧设计**(红箭头/AddCastPower 直接瞬发)，其源码**已删除**。当前采用的是 **§2.1 的新规格**(交换槽→施放→还原)。旧记录保留作参考。

## 2. 当前状态与进度

- [x] 确定项目定位(Skyrim SE 1.6.1170，CommonLibSSE-NG + Address Library)。
- [x] 初始化仓库/项目骨架 (CMake, CommonLibSSE-NG 依赖, 插件入口 `SKSEPlugin_Load`)，**CMake 配置通过、插件 DL 编译成功** ✅。
- [x] 定义插件的具体功能需求：**X 键第二槽(见 §2.1 新规格，临时换槽→AddCastPower→还原)** ✅。
- [x] 编写最小可用源码骨架并编译通过。
- [x] 明确的项目结构已被记录进 `DATA.md`。

> 更新规则：完成里程碑时，勾选并移入「已完成记录」，新增待办写进下一节。

**第二槽底层选址（已实测 + 定稿极简方案）**：曾为"第二个龙吼槽位"做深入逆向，结论与决策如下——
- **实测（SEMod.log 探针）**：玩家按 X 时 4 档 `Actor::ACTOR_RUNTIME_DATA::magicCasters[0..3]` 的 `currentSpell` **全部为 null**（含 index 2 / source=2 的 kUnknown 闲置槽、source=3 的力量/龙吼槽）。证据确认 `AddCastPower` 是**瞬发式**施放，**不写入任何 caster 的 currentSpell**，也不改 `selectedPower`(0x1E0)/`HighProcessData.currentShout`(0x008)（施放前后一直是 0x0 / 固定值）。故"借引擎 caster 槽驻留第二龙吼 + 引擎自动红箭头"这条路**物理不成立**——引擎每 actor 只有 4 个 casters、唯一 currentShout/selectedPower，没有第二个原生槽，也没有"写第二槽后自动画箭头"的机制。
- **用户定稿（重构）**：不要自绘红箭头，**用原版**。最终方案极简：魔法菜单里按 X 记住当前高亮龙吼/特殊能力存 `m_secondSlot`(TESForm*)；平时按 X 用原版 `PlayerCharacter::AddCastPower(该槽的 SpellItem*)` 施放，**不改任何当前选中**，对 Z 键/原槽零污染。已在 `SecondSlotManager::CastSecondSlot()` 重构并编译通过（清理了原探针与过度诊断日志）。

**X 键第二槽插件进度**：第 1 步(第二槽+X键+AddCastPower 施放) ✅ 完成；第 2 步(X 键场景分化存槽) ✅ 完成；第 3 步(魔法菜单红色箭头) ✅ 完成（已按实测二次重写架构）。三步齐备，首次实测(SEMod.log)精确锁定了两个症状的成因：

- **施放其实是通的**：日志显示每次按 X，`AddCastPower(旋风冲刺)` **都被反复调用**、法术名也正确解析 → 施放路径本身**无 bug**。`战斗控制=false` 恒为假是 `IsFightingControlsEnabled()` 这个查询本身在 1.6.1170 不可靠（误导），既非玩家状态也非代码缺陷——玩家此前很可能一直处于战斗控制本就为 false 的区域。**判定：施放功能正常，无需再改。**
- **箭头从来不出现的真因（已修复）**：诊断日志 `selectedIndex=103 但 itemCount/entryCount/dataCount 全 = -1` 证明：MagicMenu swf 的 `itemList` 对象**能下标读 `entryList[selectedIndex]`（选中/存储路径可靠）但 `GetLength` Invoke 读不到长度**。原先箭头用"遍历列表按 formID 匹配"必须先拿长度 → 永远拿不到 → 永远匹配不上 → 箭头永不出现。**这是架构缺陷，不是微调问题。**
- **修复方案（已落地）**：放弃遍历/匹配。改由**存槽瞬间**捕获选中项的渲染坐标（`entryList[selectedIndex]` → elem → renderer._x/_y，与已证明可靠的选中读取同一路径），箭头**直接画在缓存坐标**，零扫描。存槽时刻菜单正开着、该项正是高亮行，坐标即箭头位置。新接口：`MenuMouseIntercept::ResolveHighlightedSlotPosition(x,y)` → `MagicMenuMarker::SetSlotPosition(hasPos,x,y)`。缺点：用户滚动列表后箭头不跟随该项（固定位），是与原版"箭头钉在选中行"一致的可接受取舍。

> 结论：施放无需再改；箭头架构已从"扫描匹配"改为"坐标缓绘"。**待用户用新 DLL 重测一次**——存槽后重开菜单，红色箭头应立即出现在第二槽项旁。

---

## 2.1 重构决定 (2026-重建, 从空白重建)

> ⚠️ **源码曾被清空重来**：第二槽插件整套源码被删除后按用户新规格从零重建。**旧"第二槽+红箭头"设计整体弃用**，只保留 CommonLib 地址/结构研究与构建链。

- **新规格(用户定稿, 伪流程)**：龙吼/特殊能力引擎原生只能 Z 键施放、一次一个。构建"X 键第二槽"：
  - **步骤 1**：魔法菜单中按 X → 记录鼠标悬停/高亮的龙吼或特殊能力(存 `TESForm*`)。
  - **步骤 2**：游戏内按 X → **临时把记录项换成当前 Z 槽**(`selectedPower`/`currentShout`) → 触发 Z 键施放函数 `Actor::AddCastPower` → **施放完成后还原原 Z 槽**，对玩家当前选中零残留。
- **机制确认(用户明确选 "临时换槽→触发Z→还原" 而非旧的 AddCastPower 直接瞬发)**。
- **CommonLib 关键 API(已核对版本)**：
  - `Actor::AddCastPower(SpellItem*)` — RELOCATION_ID(37787, 38736)，即 Z 键实际施放函数。
  - `Actor::ACTOR_RUNTIME_DATA::selectedPower`(TESForm*, 0x1E0) 力量槽；`HighProcessData::currentShout`(TESShout*, 0x008) 龙吼槽；`magicCasters[4]`(0x1A0)；`AIProcess::high`(0x010)。
  - `TESShout::variations[3]`(0x60，每个含 `SpellItem*`)，施放龙吼取 `variations[0].spell`。
  - `ActorMagicCaster::SetCurrentSpellImpl` 为空实现 → 施放不依赖常驻 currentSpell。
- **重建后的源码文件**(见 DATA.md)：`src/Version.cpp`(SKSEPluginInfo，加载必需)、`src/main.cpp`、`src/SecondSlotManager.{h,cpp}`、`src/MenuMouseIntercept.{h,cpp}`。**红箭头(MagicMenuMarker)已移除**，CMakeLists 同步删其源文件。

---

## 2.2 阶段性成果 (2026-10-13 首次实测可用)

> **X 键第二槽插件已实测运行**：魔法菜单按 X 存高亮龙吼/力量，退出菜单按 X 施放——均正常触发（用户确认「可以使用了」）。这是本项目首个能在游戏内跑通的功能版本。过程踩坑与最终结论如下：

### v0.3 实测链路（三段功能全部打通）
- **加载 + 日志**：`SKSEPlugin_Load` 手动初始化 spdlog（CommonLib 的 `SKSE::log::*` 只调 spdlog、从不自建 logger/file sink）→ `SEMod.log` 正常生成，这是后续所有诊断的前提。
- **X 键监听**：`BSTEventSink<InputEvent*>` 挂 `BSInputDeviceManager` 收键正常（先前的设备/事件遍历均 OK）。
- **存槽（魔法菜单开按 X）**：`ResolveHighlightedForm()` 读高亮项 → **表单 ID 在 `entry.formId`**（小写 d，number 类型，直接挂在 entry 上，无 data 层）→ `TESForm::LookupByID` → `m_secondSlot`。
- **施放（菜单关按 X）**：`MenuMouseIntercept::GetSpellFromForm(m_secondSlot)`（龙吼取 variations[0].spell，Power 直接 As<SpellItem>）→ `player->GetMagicCaster(MagicSystem::CastingSource::kInstant)->CastSpellImmediate(...)` 立即施放。

### 三条被实测证伪 / 修正的关键认知
1. **X 键判定不能靠 `QUserEvent()==xButton`**：键位布局改动后物理 X 键（idCode=45=DIK_X）的事件名是**空字符串** `''`（未绑定命令名），`xButton`("XButton") 永远匹配不上 → 静默无反应。**改为按物理扫描码 `button->GetIDCode() == REX::W32::DIK_X` 判定**，与键位绑定无关。
2. **施放必须走 `MagicCaster::CastSpellImmediate`**（`GetMagicCaster(kInstant)`），**绝不可直写 voiceState/currentShout**（引擎不采纳、无动作）。此前用 `Actor::AddCastPower`（临时换槽→施放→还原）也被证伪，最终确认 **CastSpellImmediate 是唯一可靠原语**。
3. **魔法菜单 swf 字段名实测**：**没有 `rootList`**（路径1取不到）；真实路径是 `Menu_mc.inventoryLists.panelContainer.itemList.entryList[itemList.selectedIndex]`；**selectedIndex 属于 `itemList` 而非 entryList**；条目对象上 **`formId`（小写 d）/ number，且无 `data` 层**——旧代码找 `data.formID` 全错。

### 反思（后续迭代方向）
- **全程靠日志逐步逼出字段名**极费轮次：每加一条诊断日志需用户重测一次。已积累可直接复用的魔法菜单结构结论（见 §2.2 第3条），下个菜单类/读取类功能可免走弯路。
- **spdlog 不初始化=日志为空 API** 是曾误导多轮的核心根因（"没有日志生成"），现已一劳永逸修复。

### v1.1（2026-10-14 四大 bug 修复）

> 用户实测 v1.0 反馈四项，全部落地并编译通过（604160B，待实测）。实现细节见 §3 决定日志 `2026-v1.1`。

1. **进游戏首次按 X 龙吼直接三阶**：根因=首次施放时 `high->currentShoutVariation` 是脏值 0xFFFFFFFF（=VariationIDs::kNone，见实测日志 4294967295）、`voiceTimeElapsed` 残留；带着脏状态注入 Z 键蓄力 → 引擎高速定级三阶。**修复**：`BeginNativeShout` 注入前净化 `currentShoutVariation=kOne` + `voiceTimeElapsed=0`（⚠️ 只动变体/计时字段，**绝不写 voiceState**——铁律）。
2. **首次施放后 Z 键管理的龙吼变 X 龙吼**：与 1 同根（脏状态致施放异常、还原错乱污染 Z 槽）；净化 + 既有 `RestoreCurrentShout` 条件还原（voiceState 归零才归位 currentShout+selectedPower）解决。
3. **第二槽不持久化**：新增 AKFS.ini `[Settings] SecondSlot`（8 位大写 hex）；`StoreHighlightedToSlot` 存槽即 `SetSecondSlot` 持久化；`RegisterInputSink` 在 kDataLoaded 后 `SecondSlotFormID`+`LookupByID`+`IsZCastForm` 校验还原。重进游戏第二槽标记保留。
4. **X 施放力量无视冷却**：能力无 recoveryTime（CommonLib 无此字段）；v0.19 单一 `m_powerBlockUntil` 被所有能力共享（能力 B 会被能力 A 的冷却误拦）。**修复**：改为**按 formID 独立**冷却 `std::unordered_map<uint32_t, steady_clock::time_point> m_powerCdUntil`——能力与龙吼解耦、能力间也互不共享（用户定稿：特殊能力冷却独立于龙吼且互不共享）。能力施放不回写 voiceRecoveryTime（避免反向污染下次龙吼）。删除 `m_powerBlockUntil`。

## 3. 已确定的决定 / 已否决的方向 (Decisions Log)

| 日期 | 决定 / 否决 | 结果 |
|------|-------------|------|
| (起始) | 技术路线定为 CommonLibSSE-NG + Address Library，以支持 1.6.1170 通用版本 | ✅ 已采用 |
| 2026-搭建 | 依赖不走 vcpkg（vcpkg 无 commonlibsse-ng 端口且本机网络失败），改 **git clone→thirdparty/ + spdlog 预 install** | ✅ 已采用 |
| 2026-搭建 | 本机生成器锁定 **`VS 17 2022` + `-A x64 -T v145`**（Ninja 卡 ABI 探测；v143 报 MSB8020） | ✅ 已采用 |
| 2026-搭建 | 插件 DLL 输出目录定为 **`build/out/<config>/`**（MODULE 库需记 `LIBRARY_OUTPUT_DIRECTORY`，见 §4） | ✅ 已采用 |
| 2026-需求 | **X 键第二槽插件（Z 键能力键改造）**：原版 Z 键同时负责龙吼(Shout)+特殊能力(Power)，来回切槽繁琐。实现独立第二槽绑新键(默认 X)，免去频繁开菜单切换 | ✅ 已采用 |
| 2026-需求 | **Z 键机制两条独立槽**：龙吼 `HighProcessData::currentShout`(0x008)、力量 `Actor::ACTOR_RUNTIME_DATA::selectedPower`(0x1E0)。Shout+Power 共用 voice(吼叫)施放通道 | ✅ 调研确认 |
| 2026-需求 | **X 键直接施放**：玩家 voice caster 是 `ActorMagicCaster`，其 `SetCurrentSpellImpl()` 为空实现 → 施放不依赖 caster 常驻 currentSpell。第二槽 X 可直接调 **`Actor::AddCastPower(SpellItem*)`** 施放，无需改写/污染游戏当前槽 | ✅ 调研确认(重大利好) |
| 2026-需求 | 第二槽两类都能存(TESShout*/SpellItem*)；选中方式=魔法菜单龙吼/能力页 **Shift+左键** 存当前高亮项；**冷却不处理**(沿用原版 voice 冷却，为平衡) | ✅ 已与用户对齐 |
| 2026-需求 | **变身解除机制关键**：原版解除变身=**再施放同一个变身 spell**(effect 检测再施放→失效，`MenuControls::beastForm` 标志翻转)。变身由 `WerewolfEffect`/`VampireLordEffect` active effect 驱动；`BGSDefaultObjectManager::kWerewolfSpell` 放狼人变身 spell。**谁放的谁解除** → X 槽变身也必须走原版通道才能解除 | ✅ 机制确认 |
| 2026-需求 | **变身解除决策**：`谁放的就谁解除，第二槽必须与原槽机制相同`。→ X 槽第二槽的施放**必须走 `Actor::AddCastPower` 原版通道**(与 Z 同一函数)，从而自然获得"X 变身后再按 X 即解除"，不引入任何 hack。不做"变身中按 X/Z 都能解"的增强 | ✅ 已与用户锁定 |
| 2026-需求 | **第二槽可见反馈**：也要一个**红色箭头**标记（区分原版 Z 槽默认色箭头）。原版箭头是 Flash 菜单脚本(`.swf` AS)渲染时加的，引擎 C++ 无"选中标记"数据 → 需 hook `MagicMenu::ProcessMessage` 注入 Scaleform `GFxValue` 脚本，在第二槽列表项前渲染红色箭头 | ✅ 已与用户对齐 |
| 2026-需求 | ~~Shift+左键存入~~（**已废弃**，见终稿：改 X 键场景分化存槽） | ❌ 废弃 |
| 2026-需求 | 实现顺序 **1→2→3**：①第二槽数据+X键监听+`AddCastPower`施放(最小可用) ②Shift+左键存入+通知 ③魔法菜单红色箭头注入 | ✅ 既定推进顺序 |
| 2026-实现1 | **第 1 步已落地**：新建 `src/SecondSlotManager.{h,cpp}`，继承 `BSTEventSink<RE::InputEvent*>` 挂 `BSInputDeviceManager` sink，监听 X 键(userEvent=="XButton")；从中解析出 `SpellItem*`(龙吼取 `variations[0].spell`)，调 `PlayerCharacter::AddCastPower(spell)` 施放。`main.cpp` 在 `kDataLoaded` 注册。Release 编译通过，导出 `SKSEPlugin_Load` 已验证 | ✅ 完成 |
| 2026-实现1 | **第二槽 = TESForm***，可存 TESShout*/SpellItem*；X 键监听匹配 `buttonEvent->QUserEvent()==UserEvents::xButton` 且 `device==kKeyboard` 且 `IsDown()`(按下瞬间) | ✅ 已确认实现 |
| 2026-实现2 | **第 2 步(终版)完成**：X 键按 `UI::IsMenuOpen(MagicMenu)` 分化——菜单开=`StoreHighlightedToSlot()`(经 `MenuMouseIntercept::ResolveHighlightedForm`+`IsZCastForm` 判定 Z 键类后 `SetSecondSlot`+`DebugNotification`)；菜单关=`CastSecondSlot()`。~~Shift+左键~~已弃 | ✅ 完成(编译通过, 待实测) |
| 2026-实现3 | **第 3 步已落地**：新建 `src/MagicMenuMarker.{h,cpp}`——hook `MagicMenu::ProcessMessage`(vtable 槽位 4，`REL::Relocation vtable.write_vfunc`), 在 `kShow/kReshow/kScaleformEvent/kInventoryUpdate` 时调用 `ApplyMarker`；遍历 `Menu_mc.rootList` 逐项比 `formID`==第二槽，命中即用 `CreateEmptyMovieClip("__dshSlot2Arrow")`+`beginFill/moveTo/lineTo` 画红色右箭头(0xFFFF0000)定位到该项左侧(坐标用 renderer `_x/_y`)，未命中则清空图形。`SecondSlotManager::SetSecondSlot` 同步调 `SetHighlightFormID`。`main.cpp` 在 kDataLoaded 一并注册。**⚠️ AE(1.6.1170) 必须取 `VTABLE_MagicMenu[0]`**(其 aeID=215652；`[1]` 的 aeID=0，会解析成地址 0 → "vtable 解析失败") | ✅ 完成(编译通过, 待实测) |
| 2026-终稿 | **放弃"右键拦截/选中即可存槽"，改为 X 键场景分化**：用户最终规格——魔法菜单打开时按 **X** 把鼠标悬停/高亮的项存入第二槽(仅 Z 键类: TESShout 或 kPower/kLesserPower/kVoicePower)，平时按 X 施放第二槽。**关键教训**：魔法菜单原生右键走 .swf 的 `NotifyMouseState`，不经 BSInputDeviceManager sink、也不经 IMenu::ProcessMessage → `return kStop` 拦不住；detour vtable 槽 0x2F 的 `NotifyMouseState` 更在部分环境崩溃(日志只见 kDataLoaded 后 "Failed to handle allocation request")。故**彻底移除所有 JSocketMouseState detour 与右键拦截**，本插件对原生输入零干扰，仅响应 X 键 | ✅ 已落地(编译通过) |
| 2026-终稿 | **箭头标记兼容性定论**：原版"收藏星标"有独立存储状态(`MagicFavorites::SetFavorite` 写 isFavorited)，但"Z 槽选中箭头"**不存在独立状态**——它是 .swf 渲染时把"当前行==currentShout/selectedPower"当场比出来的。因此**不存在可白嫖的原版第二槽箭头状态**；要"复用箭头"只能 hook 渲染层(崩溃雷)。故第二槽箭头=**纯 Scaleform `CreateEmptyMovieClip` 自绘红色右箭头(独立实例 `__dshSlot2Arrow`)**，渲染逻辑与游戏零耦合 → 天然兼容、不崩溃 | ✅ 已落地 |
| 2026-实现4 | **精简 MenuMouseIntercept 为纯查询工具**：`src/MenuMouseIntercept.{h,cpp}` 移除 `EnsureMovieHook`(NotifyMouseState detour)与全部 hook，只保留 `IsZCastForm`(Shout 恒真; Spell 的 kPower/kLesserPower/kVoicePower)与 `ResolveHighlightedForm`(MagicMenu 打开 → `Menu_mc.inventoryLists.panelContainer.itemList` → selectedIndex → entryList[index] 或 getItemAt → item.data.formID → TESForm::LookupByID)。`SecondSlotManager` 的 X 键经此解析/判定；`StoreHighlightedToSlot` 替代被弃的右键 store。`MagicMenuMarker::ProcessMessageHook` 不再调 EnsureMovieHook | ✅ 完成(编译通过, 待实测) |
| 2026-加载修复 | **首次真正被 SKSE 加载的教训**：CLibSSE-NG 插件必须在某翻译单元里调用 `SKSEPluginInfo({...})` 宏以导出 `SKSEPlugin_Version`/`SKSEPlugin_Query`。此前工程少写此块，skse64.log 报 `plugin ... no version data 0 (handle 0)` → SKSE 根本不调 `SKSEPlugin_Load` → 插件"没有效果"。新增 `src/Version.cpp`(含 `SKSEPluginInfo`，`RuntimeCompatibility=AddressLibrary`)后，dumpbin 验证导出 `SKSEPlugin_Load`+`SKSEPlugin_Version`+`SKSEPlugin_Query`，SKSE 正常加载 | ✅ 修复并实测(SEMod.log 生成) |
| 2026-改进 | **v0.4 冷却 + 长按蓄力龙吼**（用户提出两项改进）：①v0.3 `CastSpellImmediate` 跳过了引擎 voice 冷却→可无限连发破坏平衡；②按下即施放、无法长按发更高级别龙吼。**方案（策略 B，仍走可靠原语）**：施放前用 `player->GetVoiceRecoveryTime()`(引擎门面,读 `high->voiceRecoveryTime`) 做冷却门禁；龙吼按下只记时蓄力、松开按时长选 `TESShout::variations[VoiceID].spell`(阈值 kChargeLevel2At=1.0s/Level3At=2.5s, MVP 可调)施放；施放后**回写 `player.GetActorRuntimeData().currentProcess->high->voiceRecoveryTime = 该级 variations[level].recoveryTime`**（力量用固定 `kPowerRecoveryTime=3.0s`），让引擎 UI"声音未恢复"与原版 Z 键及下次 X 都尊重该冷却。X 按下/释放用 `m_wasDown` 状态变化沿判定(BSInputDeviceManager 对键盘按下/释放各回调一次)。**⚠️ 编译坑**：`DebugNotification` 在 `RE/M/Misc.h`；`currentProcess` 是 `Actor::ACTOR_RUNTIME_DATA`(0x0F0) 成员，须经 `GetActorRuntimeData().currentProcess` 访问，不是 Actor 直接成员 | ✅ 完成(编译通过, 待实测) |
| 2026-v0.5 | **冷却采纳 + 蓄力改走"原版蓄力"**（用户实测 v0.4 反馈"冷却做的好，蓄力不行，应改用原版蓄力；默认按下即触发且未用原版蓄力"）。经 `ask_user_question` 确认 **Z 键为默认布局(DIK_Z=0x2C)**、选**完整注入原版 Z 键通道（推荐）**。**实现**：`src/SecondSlotManager.{h,cpp}` 重写——龙吼按 X 按下沿 `StartNativeShoutCharge()`：先在 `high->currentShout` 临时填入第二槽龙吼(存原值 `m_previousShout`)，再 `ButtonEvent::Create(kKeyboard, UserEvents::shout, DIK_Z, 1.0F, 0.0F)`(IsDown) 喂给 `PlayerControls::GetSingleton()->shoutHandler->ProcessButton(&pc->data)` → **引擎自己进入原版蓄力**（音效/升级/动画全原生）；松开沿 `FinishNativeShoutCharge()`：注入 `ButtonEvent::Create(..., 0.0F, heldSecs)`(IsUp，heldSecs=真实按住时长)→ 引擎按蓄力时长**分级施放并自动设原版冷却**，随后还原 `high->currentShout`。力量非龙吼不蓄力：仍 `CastSpellImmediate`(kInstant) 立即施放 + `SetVoiceCooldown(kPowerRecoveryTime=3.0s)`。冷却门禁沿用 v0.4 `OnCooldown()`。**⚠️ 编译坑**：头文件需前向声明 `class TESForm;`(成员 `m_secondSlot` 用)；`Player()` 改 `const`（`OnCooldown() const` 内部调用它）。**⚠️ 风险**：`ShoutHandler::ProcessButton` 是引擎内部入口（CommonLib 无 .cpp），注入是否在 AE 真正触发原版蓄力需用户实测 | ✅ 完成(编译通过, 583680B) |
| 2026-v0.6 | **⛔ v0.5 注入通道被用户实测否决，回退到自算蓄力**。用户三项回归：**①总是有延迟；②丢失瞬时按键触发一阶段（点按不即时出 1 级）；③有时卡住**——释放不出来的同时提示"使用龙吼"，导致原生 Z 键龙吼也失效。根因：`ProcessButton` 手工注入绕过了引擎真实的按-持-放状态机，且 `high->currentShout` 交换在异常路径残留 → 引擎卡在原版蓄力/龙吼占用态。**决策**：**弃用 `ShoutHandler::ProcessButton` 注入与所有 `currentShout` 改写**（不再改动任何引擎 voice 状态），回到 v0.4 自算蓄力（本就获"冷却做得好"认可）+ `CastSpellImmediate`（唯一已证可靠原语，v0.3 实测）。新实现：龙吼按下只记时(`m_shoutActive`/`m_shoutStart`)，松开按 `kChargeLevel2At=1.0s/kChargeLevel3At=2.5s` 选级(瞬时点按落 1 级)取 `TESShout::variations[level].spell` 经 `CastSpellImmediate` 施放，回写原版冷却 `variations[level].recoveryTime`；力量不变。删除成员 `m_previousShout`、`Start/FinishNativeShoutCharge`、龙吼比 3 级 `variations` 三份防御越界；字段确认（CommonLib `RE/T/TESShout.h`）`Variation{SpellItem* spell@08, float recoveryTime@10}` | ✅ 完成(编译通过, 584704B) |
| 2026-v0.7 | **v0.6 又被用户实测否决：无吼声/无抖动（"感觉空"）。改回引擎原版施放函数拿吼声**。用户补充"改进0.5版本，感觉是因为抢了什么函数导致的"——指向 v0.5 注入抢了引擎函数流/currentShout。**决策**：保持 v0.6 的自算蓄力选级（无注入、无延迟），但把龙吼施放原语从 `CastSpellImmediate` 改回 **`Actor::AddCastPower(SpellItem*)`**（Z 键原版施放入口，v0.2 首次实现即用它、实测施放可靠，且实测**不写 caster.currentSpell / selectedPower / high->currentShout**——零状态污染、不抢函数、不卡原生龙吼）。`AddCastPower` 走引擎 Shout 通道施放 → 得到**原版吼声 + 抖动**；力量分支不变(仍 CastSpellImmediate+固定冷却)。src=584704B | ✅ 完成(编译通过待实测) |
| 2026-v0.8 | **用户实测 v0.7(AddCastPower)在 AE 上只弹提示不真施放，也无吼声**；选 E 走引擎原版 Shout 通道。**实现**：`SecondSlotManager` 重写——龙吼走 `ShoutHandler::ProcessButton` 注入 Z 键 down/每帧 held(heldSecs=真实累计)/up；`high->currentShout` 临时换第二槽、release 还原；新增 `BeginNativeShout/FeedNativeHeld/FinishNativeShout/RestoreCurrentShout`（替代 v0.7 的 `Start/FinishShoutCharge` self-charge）。src=584192B。**⚠️ 用户实测回归(Confirmed 最新 v0.8)**：①**快按 X 只蓄力不出一级**（引擎原生蓄力需真实按住积累，点按本就不出 1 级=原生逻辑）；②**施放的还是 Z 槽龙吼而非第二槽**。用户关键记忆："**0.5 版本里是可以选择其他龙吼**" → 换 currentShout+喂 Z 键事件这条路是通的，v0.8 变错是**每帧 FeedNativeHeld 期间引擎把 currentShout 换回玩家选中槽**所致 | ✅ 完成(编译通过, 待 v0.9 实测) |
| 2026-v0.9 | **修复 v0.8 两项回归**：在整个蓄力期（每帧 FeedNativeHeld + FinishNativeShout 喂 up 前）**持续把 `high->currentShout` 钉回第二槽**（防御引擎 held 期间重解析换回选中槽，diff 时打 warn 日志），up 施放后打 `player->GetCurrentShout()` 诊断日志确认引擎读到哪个龙吼。src=586240B。⚠️ 快按 X 本就不出一级（原生蓄力需真实按住），属预期；重点验证"松开后施放的是第二槽" | ✅ 完成(编译通过, 待实测) |
| 2026-v0.10 | **用户实测 v0.9：能切换龙吼了 ✅；但只剩三阶能施放，一二阶卡住——"似乎在等第三阶"**。诊断：引擎三阶是**同步结算**（松开当帧出），一二阶是**异步结算**（松开后跨帧才完成施放）。v0.9 在 FinishNativeShout 喂完 up 后**立即 RestoreCurrentShout** → 引擎完成早阶施放时 currentShout 已还原成原 Z 槽 → 施放成 Z 槽/卡住；三阶同步在还原前结算所以能出。**修复**：改为**延迟还原**——FinishNativeShout 喂 up 后不立即还原，置 `m_restorePending=true; m_restoreAt=now+800ms(kShoutRestoreDelay)`，`currentShout` 保留第二槽让异步结算吃到；到期/下次 X 按下/存槽前由新增 `TryDeferredRestore()` 兜底还原。src=587776B | ✅ 完成(编译通过, 待实测) |
| 2026-v0.11 | **用户实测 v0.10：蓄力分级/切换都 ✅，但报两点**：①施放后需**再按一次 X 解除使用状态**、再按一次才可施放（开关效应，仅测了可分段的龙吼）；②**吼声用的是 Z 选中的龙吼声音**。**尝试修复**：②施放期同步换 `Actor::ACTOR_RUNTIME_DATA::selectedPower(0x1E0, 吼声/HUD身份)`；①up 后 `high->voiceState` 复位 `kNone`。src=588288B | **⛔ 用户实测卡在动作放不出来**（selectedPower 与 voiceState 叠加，当时误判 selectedPower 为凶手） |
| 2026-v0.12 | v0.11 卡动作后**误判 selectedPower 换槽为凶手**，撤销 selectedPower 换槽、保留 voiceState kNone 复位 → 仍卡。**用户最终定位：真正凶手是 voiceState kNone 复位**。 | ⛔ 排除错误方向 |
| 2026-v0.13 | **真凶=voiceState kNone 复位**（v0.12 实测卡动作）；selectedPower 换槽也可能叠加但以 voiceState 为主。**修复**：**摒弃一切 voice 字段附加写入**（selectedPower 换槽、voiceState 复位全撤），回到 v0.10 纯 `currentShout` 换槽（施放/分级一切正常）。issue-1（多按一次 X 开关效应）、issue-2（吼声 Z 槽）**未解决**且已证不能靠 voice 字段写入修正，需完全不碰 voice 状态的方案另行研究。src=587776B | ✅ 完成(编译通过, 待实测) |
| 2026-v0.15 | **用户实测 v0.14：换吼声/施放特殊能力问题已解决 ✅；剩"需再按一次 X 解除、menu 变红、Z 选槽来回跳"**。**根因分析**：①变红=selectedPower 是菜单/HUD"当前选中/使用中"身份，X 施放 up 后延迟还原期间仍=第二槽 → 开龙吼菜单被当"使用中"变红；原版 Z 施放不动 selectedPower → 不变红（X/Z 差异根源）。②来回跳=延迟还原兜底 `RestoreCurrentShout` 用已清零的 `m_previousPower`=null 再写 `rt.selectedPower`，清掉玩家 Z 选龙吼；加上菜单按 X 的 `StoreHighlightedToSlot` 重存槽交错。**修复**：**解耦**——selectedPower 施放 up 后**立即**还原回玩家 Z 选龙吼（m_previousPower 即时清零）；currentShout 仍延迟 800ms（异步早阶结算要读）；`RestoreCurrentShout` 只还原 currentShout。src=588288B | ✅ 完成(编译通过, 待实测) |
| 2026-v0.17 | **用户实测 v0.16 诊断版后确认"红字解决了"✅**。**修复**：把"固定 800ms 还原 currentShout"改为**条件还原**——`TryDeferredRestore` 每帧判定：voiceState 归零(kNone=施放彻底结束)才还原，否则挂着并重钉 currentShout=第二槽，直到 m_restoreAt(3s 硬上限)兜底。红字由 currentShout/voiceState 决定（与 selectedPower 无关，v0.15 立即还原 selectedPower 仍红字可证）。1/2 阶声音仍错(见 v0.18) | ✅ 完成(编译通过, 红字实测通过) |
| 2026-v0.18 | **用户实测 v0.17 后 1/2 阶声音仍错（1 阶=Z 槽 1 阶声；2 阶=X 槽蓄力声后接 Z 槽 2 阶声），3 阶正确，红字保持不回归**。**根因（2026-10-13 SEMod.log 实锤）**：引擎**异步施放(1/2 阶)的声音来源是 `selectedPower`(0x1E0) 而非 currentShout**——3 阶同步在 up 帧内 selectedPower=第二槽→X 声；1/2 阶异步跨帧施放时 selectedPower 已被 v0.15/v0.17"立即还原"回玩家 Z 槽(00070980)→播 Z 声；2 阶"先 X 蓄力声(hold 期 currentShout=第二槽)再 Z 二阶段声(异步施放读回 selectedPower=Z 槽)"完全吻合。**修复**：selectedPower **不再立即还原**，与 currentShout 一起纳入条件还原（pending 每帧两字段都重钉回第二槽，voiceState 归零或 3s 硬上限才一起还原）。`RestoreCurrentShout` 已扩展为同时还原 currentShout+selectedPower(仅当 m_previousPower 非空，避免把玩家既有 Z 槽清成 null)。src=593408B | ✅ 完成(编译通过, 待实测) |
| 2026-v0.19 | **龙吼全阶实测通过(v0.18)✅。剩余两个特殊能力问题，用户 2026-10-13 实测**：①**Z=龙吼、X=能力时，龙吼冷却中 X 能力放不出**——根因：`OnCooldown()` 只走引擎 voice 冷却(GetVoiceRecoveryTime)单一门槛，龙吼施放占用它 → X 能力被当年 voice 冷却拦截。**修复**：冷却按槽类型分流——能力槽走插件独立 `m_powerBlockUntil`(steady_clock)，与 voice 冷却完全解耦；能力施放也不再回写 voiceRecoveryTime。龙吼槽仍走引擎 voice 冷却(与 Z 一致)。②**吸血鬼领主下 X→恢复形态(Revert Form)无效；随后正常选恢复形态按 Z → 崩溃**（崩栈在 armor/BipedAnim attach 路径，hdtsmp/hair 等第三方 mod 参与，变身状态切换+铠甲重挂交错）。**修复**：`CastPowerNow` 施放前先清掉任何残留的龙吼延迟还原(restorePending→RestoreCurrentShout 归位)，能力施放本身不碰 currentShout/selectedPower/voiceState。删除不再使用的 `SetVoiceCooldown`。src=592384B | ✅ 完成(编译通过, 待实测) |
| 2026-v0.20 | **用户实测确认"喜欢菜单"=按 Q 弹出的 FavoritesMenu 侧菜单；并要求在其中按 X 也能选中存第二槽**（此前只识别 MagicMenu → Q 菜单里按 X 走施放路径无效果、退出菜单还误释放）。**修复**：①选槽菜单识别扩展为 **MagicMenu 或 FavoritesMenu**；②FavoritesMenu 收藏项直接存 C++ 侧 `rt.favorites`(BSTArray<Entry>，Entry.item=TESForm*)，高亮索引从 swf `root.selectedIndex` 读，越界/读不到兜底取"第一个 Z 键类收藏项"(`MenuMouseIntercept::ResolveFavoritesHighlightedForm`)；③新增 `m_storePendingRelease`——本次 X 在菜单里用于存槽则抬起时不施放(防退出菜单瞬间误施放)。src=595456B | ✅ 完成(编译通过, 待实测) |
| 2026-改名 | **用户定稿品牌**：插件显示名/Author = **"Another Key for Shout"**，日志与通知前缀缩写 **`AKFS:`**。Version.cpp 的 `.Name/.Author` 改 "Another Key for Shout"→ 日志文件自动变 `Another Key for Shout.log`。**移除施放成功提示**（龙吼/能力两条 `Notify("已施放第二槽")`），保留存槽成功/冷却/警告提示（前缀改 `AKFS:`）。**DLL 改名 `AKFS`**（CMake `PLUGIN_NAME` "SEMod"→"AKFS"，输出 `build/out/Release/AKFS.dll`；部署覆盖游戏 Plugins 的 **`AKFS.dll`**，副本 `E:\SE mod\AKFS_latest_for_game.dll`）。⚠️ **C++ 命名空间 `SEMod` 保持不变**（含空格不能作标识符，且与 DLL 名解耦无需改）。src=594944B | ✅ 完成(编译通过, 待实测) |
| 2026-配置 | **ini 配置驱动 + FOMOD 选语言**（用户需求：提示英文版 + 装时选语言 + 可改键位）。新建 `src/AKFSConfig.{h,cpp}`：Win32 `GetPrivateProfileString/Int` 读插件 DLL 同级 `Data\SKSE\Plugins\AKFS.ini`。ini 键：`[Settings] KeyCode`(DIK 扫描码, 默认 0x2D=45=X；⚠️ DIK_X=0x2D=45 非 57) 与 `[Lang] Language`(English/中文)。DLL 自身目录=`GetModuleHandleW(L"AKFS.dll")`→`GetModuleFileNameW`→parent（不是 REL::Module::get()，那是游戏 exe 路径）。`SecondSlotManager::RegisterInputSink` 触发 `AKFSConfig::Load()`；X 键判定 `GetIDCode()==KeyCode()`；提示改 `NotifyLocalized(cn,en)` 按 `English()` 选。⚠️ **教训：`<windows.h>` 绝不能放头文件**——macro 污染（`small`/`min`/`max`）导致 CommonLib `TESFile.h` `C2229 大小为零数组` 编译错；仅放 .cpp(放 AKFSConfig.h include 之后)。FOMOD 包 `deploy/`：`fomod/ModuleConfig.xml`(installSteps 下拉选 简体中文/English，`<file source="/{Chinese,English}/AKFS.ini" destination="/SKSE/Plugins/AKFS.ini"/>` 单选写 ini) + 根 `AKFS.dll`(恒装)。默认 ini `deploy/AKFS.ini` | ✅ 完成(编译通过, src=599552B, 待实测) |
| 2026-v1.1 | **四大 bug 修复（用户实测反馈 v1.0）**，编译通过 604160B，待实测。详见 §2.2 v1.1。**①首次按 X 龙吼直接三阶**（根因：进游戏首次施放时 `high->currentShoutVariation` 是脏值 0xFFFFFFFF=kNone、`voiceTimeElapsed` 残留，带着脏状态注入 Z 键蓄力 → 引擎高速定级到三阶）。**修复=`BeginNativeShout` 注入前净化**：`currentShoutVariation=kOne` + `voiceTimeElapsed=0`（⚠️ 只动变体/计时字段，**绝不写 voiceState**铁律）。**②首次施放后 Z 键管理的龙吼变 X 龙吼**（同根，脏状态致施放异常、还原错乱污染 Z 槽；净化+既有 `RestoreCurrentShout` 条件还原解决）。**③持久化**：新增 AKFS.ini `[Settings] SecondSlot`（8 位大写 hex；`SetSecondSlot` 写 / `SecondSlotFormID` 读），`StoreHighlightedToSlot` 存槽即持久化、`RegisterInputSink` 加载还原（须 `IsZCastForm` 校验）。**④力量冷却被无视**：能力无 recoveryTime 字段（CommonLib 无），根因 v0.19 单一 `m_powerBlockUntil` 被能力共享。**修复**：能力冷却改**按 formID 独立** `std::unordered_map<uint32_t, steady_clock::time_point> m_powerCdUntil`（能力与龙吼解耦、能力间也互不共享冷却，符合用户定稿）；能力施放不回写 voiceRecoveryTime。删除 `m_powerBlockUntil`（仅留 v0.19 注释历史引用） | ✅ 完成(编译通过, 待实测) |
| 2026-v1.2 | ⛔ v1.1 净化修复被用户实测否决（"第一次 x 键直触发第三阶段还未解决"），并从日志实锤真正根因：**按下→松开仅一次，但 down→up 事件间隔≈6s，up 后 voiceState=6(kFail)**——用户明确"游戏认为我没松手"。**真相：X 键的 up 事件有时根本没到达事件流（BSInputDeviceManager 不发该 up），m_wasDown 恒为 true → 松手后 FinishNativeShout 永不触发 → 引擎蓄力仍按墙上时钟累计 → 冲到三阶**。净化 currentShoutVariation/voiceTimeElapsed 根本无关（改造没触碰"释放检测"）。**修复（v1.2）**：`ProcessEvent` 每帧在 TryDeferredRestore 后、事件循环外加**物理键抬起兜底**——新增私有 `IsXKeyPressed()` 用 Win32 `GetAsyncKeyState`（DIK 扫描码→`MapVirtualKeyW(MAPVK_VSC_TO_VK)`→高位 0x8000）直接查物理键是否仍按下；若 `m_wasDown && m_shoutActive && !IsXKeyPressed()`（且非 storePendingRelease/非选槽菜单）→ 立即 `m_wasDown=false` + `FinishNativeShout()`，heldSecs 取真实按住时长 → 点按落 1 阶。`inStoreMenu` 上提至事件循环前共享。⚠️ **弯路教训：先试 `BSWin32KeyboardDevice::IsPressed` 关联时 LNK2001 未解析**——CommonLib 的 BSInputDevice 相关虚函数(GetKeyMapping/IsEnabled/Initialize/Process/Release/Reset 等)实现未随 CLIBS 链接，引用即稳炸；改用 Win32 `GetAsyncKeyState` 零依赖。⚠️ `<windows.h>` 放 .cpp 末尾（DTRequire DINPUT.h 之后），避免 macro 污染（与 §3 2026-配置 同教训）。**⛔ 用户复测仍"设 X 槽后第一次按 X 出三阶 + Z 变 X 槽龙吼"**（存档/重进不影响→非启动态，是每次设槽后首次的残留）。**精确定位**：①"Z 变 X 槽龙吼"= `RestoreCurrentShout` 的 `if(m_previousPower)` 条件在 m_previousPower 为 null 时跳过 selectedPower 还原 → 残留第二槽。**修复**：selectedPower **无条件**还原为 m_previousPower（可为 null），并加 `[restore]` 前后值日志（m_previousPower 换槽记/还原用/用后置 null 严格配对，不会误清玩家选择）。②"设槽后第一次出三阶"仍= up 事件被吞 → heldSecs 过长，但 ProcessEvent 轮询在 held 流也断时执行不到。**加固**：物理松手检测加进 `FeedNativeHeld()` 开头（蓄力期每帧必被调用的可靠挂载点）——`!IsXKeyPressed()` 即 `FinishNativeShout()`（内部 m_shoutActive 防重入）。src=605184B | ✅ 完成(编译通过, src=605184B, 待实测) |
| 2026-锁根因 | **设槽后首次按 X 出三阶 ROOT CAUSE + FIXED（2026-10-14 16:02 日志实锤）**：设槽的 X 抬起设了 `m_storePendingRelease=true`；紧接着该次抬起在 up 分支 `if(m_storePendingRelease && !m_shoutActive) break` 吞掉——但此时 down 分支已 `if(!OnCooldown()) OnXDown()` 开始了真实施放（m_shoutActive=true），up 因这条守卫**永不调 FinishNativeShout** → m_shoutActive 泄漏为 true → 下一次真正按 X 走 held 用**上一次的墙钟 m_shoutStart 累计 → heldSecs≈16.8s → 引擎直出三阶**。**修复**：up 分支守卫改 `if(m_storePendingRelease && !m_shoutActive)`（仅在**未**开始真实施放时才吞 up 存槽标志）；down 分支非菜单时先 `m_storePendingRelease=false` 再 OnXDown，杜绝泄漏。**用户确认修复**。src=608256B | ✅ 用户实测通过 |
| 2026-v1.4 | **只对 kPower 套"当日一次"，可反复能力/例外组合开后门**（UESP 明文依据：Greater Powers=每天一次、Lesser Powers=可任意次；Beast Form+净化 Ring of Hircine 为"类型 kPower 却可反复"的例外）。原 v1.3 对**所有**能力套当日门禁 → 误杀可反复用的 kLesserPower(如吸血鬼领主变身/Night Eye/黑市等)。**修复**：新增 `IsDailyPower(const RE::SpellItem*)`= `${SpellType==kPower} && !DailyBypassContains(formID)`；`OnCooldown` 能力分支仅当 `IsDailyPower(槽内spell)` 才查 `PowerUsedToday`，`CastPowerNow` 也仅对 IsDailyPower 落库"当日一次"。**后门**：ini `[Settings] DailyBypass=<formID1>,<formID2>,…`(8位hex, 逗号/空白分隔, AKFSConfig 增 `s_dailyBypass unordered_set` + `DailyBypassContains`)。kLesserPower(3)/kVoicePower(11) 恒只走 3s 短冷却。⚠️ 编译坑：`IsDailyPower` 形参用 `const RE::SpellItem*` 需头文件前向声明 `class SpellItem;`（原仅有 PlayerCharacter/TESForm/TESShout），否则 C2039 SpellItem 非 RE 成员。src=613888B | ✅ 完成(编译通过, 待实测) |
| 2026-v1.7 | **SKSE Menu Framework 配置菜单（弃用 MCM 方案）**。用户先问"不用 Papyrus 能否用 MCM"，结论：**SkyUI MCM 在架构上必须挂 `SKI_ConfigBase` 的 Papyrus Quest，纯 C++ 插件接不进**（MCM 扫描的就是挂该脚本的 Quest 实例，没有魔改工作区之外的注册入口）。用户遂改选 **SKSE Menu Framework**（独立 SKSE 插件 + 纯头文件 C++ API，**无 Papyrus / 无 ESP / 无 SkyUI**），前置依赖用户已接受。**集成机制**：`SKSEMenuFramework.h`(520KB 单头) 用 `GetModuleHandleW(L"SKSEMenuFramework")` + `GetProcAddress` 运行期解析，**不链接任何库、不需 vcpkg**，CMake 只需 `target_include_directories`；`SKSEMenuFramework::IsInstalled()` 检查 `Data/SKSE/Plugins/SKSEMenuFramework.dll` 是否存在 → **可选依赖，未装则 `Register()` 直接 return，X 键主功能不受影响**。**配置架构（C 方案，用户拍板）**：业务代码只读内存静态变量（`AKFSConfig::KeyCode()` 不看 ini），故**必须改内存才立即生效**；ini 仅作"下次启动恢复"。故新增的 `Set*` 全部**内存+ini 双写**：先改内存（SecondSlotManager 零改动即生效）再 `WritePrivateProfileStringA` 落盘。**本地化（用户定稿 A 方案）**：两个完整语言 ini（`Language/Chinese/`、`Language/English/`）各含 `[Settings]`+`[Lang]`+**新增 `[Strings]` 节**，安装时由现有 FOMOD 择一拷成 `AKFS.ini` —— 因"提示文案在菜单里改不了"，故文案必须随安装语言走。`AKFSConfig::Text(key)` 查 `[Strings]`，查不到回退返回 key 本身（便于定位漏翻）。⚠️ Win32 无枚举 ini 节内键的 API，故用 `kStringKeys[]` 已知键清单逐个 `GetPrivateProfileStringA`。字体问题**用户明确甩给用户自己解决**（属 SKSE Menu Framework 自身配置，插件不管）。**新增文件** `src/AKFSMenu.{h,cpp}`；**接口扩展**：`AKFSConfig` 增 `Reload()`/`SetKeyCode`/`SetPowerOncePerDay`/`SetLanguage`/`SetDailyBypass`/`SetBlockedPowers`/`DailyBypassSet`/`BlockedPowersSet`/`Text`/`IniPath`/`ReloadStringsFrom`；`SecondSlotManager` 增 8 个纯读访问器 + `TestCastSecondSlot()`。**菜单两页**：`设置`(触发键滑块+物理键 Capture 绑定/能力每日一次/DailyBypass 与 BlockedPowers 列表增删/界面语言 Combo/重载 ini)、`状态`(第二槽 formID+名称+类型/Z 槽龙吼/龙吼与能力冷却实时倒计时/voiceState/charging/restorePending/ini 路径 + 清空槽/重载/测试施放按钮)—— **状态与动作是 ini 永远给不了的**，尤其 `voiceState`/`m_shoutActive`/`m_restorePending` 可直接观察 MEMORY 里反复踩的"卡蓄力/还原残留"类 bug。**动作投递**：按钮只置 `g_pendingXxx` 标志，由 `AddHudElement` 的每帧回调消费，**不在 ImGui 渲染回调里直接驱动游戏施放**。src=649216B | ✅ 完成(编译通过，导出 SKSEPlugin_Load/Query/Version 已验证，待实测) |


- ❌ 否决项：无(项目起步，尚未有否决记录)。

---

## 4. 关键坑与经验教训 (Lessons / Gotchas)

> 实际搭建中验证过的坑（2026 首次构建）：

- **运行时版本匹配**：Address Library 数据库必须与游戏 runtime 对应(1.6.x → `vr`，此处到 1170)。加载失败通常表现为插件在游戏中不加载，或地址解析为 0。
- **入口函数导出名**：SKSE 插件必须导出 `SKSEPlugin_Load`。拼错直接不加载。
- **不硬编码地址**：用 CommonLibSSE-NG + Address Library，永不写死游戏内部地址。
- **⚠️ 本机 Build Tools 是 VS 2026 (channel 18.x)，平台工具集是 v145，不是 v143**：
  - CMake 的 `-G "Visual Studio 17 2022"` 默认要 v143 → 报 `MSB8020 / v143`。**必须加 `-A x64 -T v145`**。
  - MSVC 工具集有 14.44 和 14.51 两套；编译用的 19.51.36256 (14.51)。
- **⚠️ Ninja 生成器在本机会卡死在 "Detecting CXX compiler ABI info"**（execute_process 子进程管道限制）。**改用 VS generator + v145 可正常通过**。
- **⚠️ 项目路径含空格 `E:\SE mod` 会处处踩坑**（cmd cd、CMake install prefix）。规避：install 用 `--prefix=<path>` 单参数；路径一律加引号；能用绝对 `-S`/`-B` 就别 cd。
- **依赖引入最终方案（不走 vcpkg）**：
  - **vcpkg 官方没有 `commonlibsse-ng` 端口**，且本机 vcpkg bootstrap 因网络/证书失败 → 弃用。
  - 改用 **git clone** 依赖到 `thirdparty/`：`spdlog`、`CommonLibSSE-NG`、`rapidcsv`。
  - spdlog 需**预 build+install** 到 `build/deps/spdlog`（`find_package(spdlog CONFIG)` 被 CommonLib 依赖）。看 `scripts/build_spdlog.ps1`。
  - rapidcsv 头文件库：`thirdparty/rapidcsv/src`（`find_path RAPIDCSV_INCLUDE_DIRS`）。
- **CommonLibSSE-NG 3.6.0 需要 C++23**(`cxx_std_23`)；默认 SE+AE+VR 多目标。
- **消息监听分阶段**：`kPostLoad` 注册 hook、`kDataLoaded` 访问数据，别在 Load 阶段读未就绪数据。
- **⚠️ MODULE 库(.dll)的输出目录在 MSVC 下归 `LIBRARY_OUTPUT_DIRECTORY`，不是 `RUNTIME_OUTPUT_DIRECTORY`**。只设 runtime 会漏掉，DLL 会落到默认 `build/Release/`。配套设 `PDB_OUTPUT_DIRECTORY`。最终产物 **`build/out/Release/SEMod.dll`**。
- **cmake.exe 不在 PATH**：需用完整路径 `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`（或在 vcvars/开发者命令行下运行）。
- **编译产物验证**：用 dumpbin `/exports` 检查导出符号是否含 `SKSEPlugin_Load`（dumpbin 在 `...\MSVC\<ver>\bin\Hostx64\x64\dumpbin.exe`）。
- **⚠️ 每个 .cpp 编译单元必须最先 `#include "SKSE/Impl/PCH.h"`**：CommonLibSSE-NG 头(如 IMemoryStoreBase.h)依赖它先建立 `std::uint32_t`/`size_t` 等环境。新写的 SecondSlotManager.h 里把它放最前，否则报 `"uint32_t" 不是 std 的成员`。
- **⚠️ 挂 BSInputDeviceManager 的 `BSTEventSink<RE::InputEvent*>`，其 `ProcessEvent` 形参是 `InputEvent * const*`**（`BSTEventSink<Event>::ProcessEvent(const Event*, ...)` 展开后 Event=`InputEvent*`）。写成 `const InputEvent*` 或 `InputEvent* const` 都不匹配基类 → 报 C3668 override 未匹配，且此类无法实例化(抽象)。
- **⚠️ 记得 include `RE/B/ButtonEvent.h` 与 `SKSE/Logger.h`**：用 `AsButtonEvent()`/`GetDevice()` 需 ButtonEvent 完整类型；`SKSE::log::*` 定义在 `SKSE/Logger.h`（只 include API.h 会报 `SKSE::log` 不是类型）。
- **⚠️ MagicMenu swf 的条目数组 `GetLength` 读不到长度**（已实测）：`inventoryLists.panelContainer.itemList` 是对象，含 `entryList` 数组成员，`entryList[selectedIndex]` 下标读**可靠**(选中/存储路径用它成功)，但 `Invoke("GetLength")`/`GetMember("length")` 对 `itemList`、`entryList`、`dataProvider` **全都返回失败**(-1) → **任何"先取列表长度再遍历"的逻辑都会失效**。教训：**不要依赖 Flash 数组的 GetLength/length 来定上界**；要么用已知索引(selectedIndex)直接下标读，要么在元素逐个 `GetElement(i)` 读到失败即停(不依赖长度)。**箭头改坐标缓绘**：存槽瞬间 `entryList[selectedIndex]` → elem → `renderer._x/_y`(回退 elem._x/_y) 捕获选中项坐标，箭头直接画在那里，零扫描、零长度依赖。
- **鼠标左键识别**：原始 `BSInputDeviceManager` 事件里，鼠标键 `ButtonEvent` 的判定用 `device==INPUT_DEVICE::kMouse && GetIDCode()==0`(DirectInput 鼠标键序号，0=左键，输入层标准，非游戏地址)。鼠标设备的事件在 CommonLibSSE 源码里只有类空壳，实际分发在游戏二进制内 → 无法从源码确认 idCode 约定，`0=左键` 按社区惯例实现，需实测(读取失败会把 kMouse 事件记日志以定位)。
- **Shift 状态**：键盘 Shift 不在 `UserEvents` 成员里，用 `REX::W32::DIK_LSHIFT(0x2A)/DIK_RSHIFT(0x36)`(include `REX/W32/DINPUT.h`) 按 `GetIDCode()` 识别；用 `IsPressed()` 赋值维持按住态。
- **魔法菜单(非 SkyUI)读取路径**(袋务上最易碎，需实测)：`UI::GetMenu<MagicMenu>()` → `GetRuntimeData().root`("Menu_mc"，GFxValue) → `GetMember("rootList")` → `selectedIndex` → `Invoke("getItemAt", idx)` → `item.GetMember("data")` → `data.formID`(AS2 Number 或 8 位 hex 字符串)。已实现**两道回退**(selectedEntry 直取 + getItemAt)与全程防御判错 + 弹"无法读取"通知，实战不符时从日志定位改字段名。
- **菜单类 vtable hook**：要给 `MagicMenu` 挂 `ProcessMessage`，用 `REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_MagicMenu[0] }`。**⚠️ 索引**：数组元素是 `REL::VariantID(seID, aeID, vrOffset)`——`[0]=VariantID(269321, 215652, 0x173ae20)` 的 **aeID=215652 非 0，AE(1.6.1170) 应取它**；`[1]=VariantID(0,0,0x173ae80)` 的 aeID=0，AE 下 `VariantID::offset()` 返回 0 → `vtable.address()==0`(先是误取 `[1]`，log 报"MagicMenu vtable 解析失败(地址为 0)")。然后 `vtable.write_vfunc(4, &Hook)` 返回原函数指针(返回值 reinterpret_cast 到函数指针类型)。`ProcessMessage` 是 IMenu vtable **槽位 4**(见 IMenu.h 注释 `// 04`)；消息类型比较用 `a_message.type == RE::UI_MESSAGE_TYPE::kX`(`stl::enumeration` 支持== 比较)，不要用 `.underlying()` 配 switch(会报 C2440)。
- **Scaleform 画矢量箭头**：在列表内 `CreateEmptyMovieClip(&clip, 固定名, depth)` 同名会重建；然后 `clip.Invoke("clear"/"beginFill"/"moveTo"/"lineTo"/"endFill")` 画三角形。**color 参数是 0xAARRGGBB**；写 double 常量时不要用 `0xFFFF0000.0`(MSVC 报 C3876 十六进制浮点无指数)，要写十进制 `4294901760.0`。moveTo/lineTo 传坐标用 size-2 的 `std::array<GFxValue,2>`。
- **在列表 child 上创建 MovieClip 传坐标**用 renderer 的 `_x/_y`(`GetMember` + `IsNumber`)；若箭头没跟住/错位，多半是 renderer 外层有 content/container clip 导致坐标空间不一致，需实测调 `kArrowGap`。不要调用列表自身 `removeMovieClip`(会删掉整个 rootList)，改以"同名重建 + 空几何隐藏"处理。
- **`RE::GFxValue::Invoke(name, result, args, n)` 是非 const 方法**：用 `const GFxValue&` 传列表参数再 `Invoke("GetLength")` 会报 C2663(this 丢失限定符)。要调 Invoke 的辅助函数其参数必须是 **非 const `GFxValue&`**(`GetMember` 是 const 可放宽，`Invoke`/`CreateEmptyMovieClip` 不是)。
- **访问 `rt.currentProcess->GetCurrentShout()` 需显式 include `RE/A/AIProcess.h`**：只 include `RE/P/PlayerCharacter.h` 会带出 Actor.h 的 `AIProcess*` 字段声明但 **未定义** `RE::AIProcess` 类型 → 报 C2027 未定义类型。而 `AIProcess::GetCurrentShout()` 返回 `TESShout*`(非 const 方法，`GetActorRuntimeData()` 的引用也必须是 `const auto&` 仍可调，因指针 pointee 非 const)。
- **dumpbin 具体路径**：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\dumpbin.exe`(在 14.51 工具集的 Hostx64\x64 子目录，不在 `...\bin\` 根)。

---

## 4.5 关键实测结论 (2026-10-12, 第三轮探针定位)

- **游戏目录**：`E:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\`
  (skse64.log 头显示的 plugin directory)。日志：`...\My Games\Skyrim Special Edition\SKSE\SEMod.log`。
- **⚠️ 部署目录长时期是"另一套"旧 DLL**：`SEMod.dll` (10/10, 590848B, 含 MagicMenuMarker + voiceState 状态机实现)
  ——**与工作区 `E:\SE mod\src\` 不同代码**。先前"按 X 无反应"的多次判断其实测的是那套，不是工作区源码。
  **更新必须手动**：把工作区 `E:\SE mod\SEMod_latest_for_game.dll`(当前构建产物副本)复制到上面 Plugins 目录
  覆盖 `SEMod.dll`。沙盒写不到游戏目录，我不代部署。
- **实测症状(用户 2026-10-12)**：那套 voiceState 版本日志显示"已存槽 0x2395a""施放请求已提交(voiceState=kRequest,
  挫志嗥叫)"，但 **游戏里人物完全没有吼/播动作** → **直接写 `HighProcessData::voiceState`/`currentShout` 引擎不采纳，
  不触发施放**。正确：解析出 `TESShout::variations[0].spell` 后经
  `player->GetMagicCaster(MagicSystem::CastingSource::kInstant)->CastSpellImmediate(spell,false,nullptr,1.0f,false,-1.0f,player)`
  立即施放(引擎最终原语)。已重构 `SecondSlotManager::CastSecondSlot` 并编译通过(2026-10-12 22:58)。

---

## 5. 待办 / 下一步 (Next Steps)

- [x] **📦 1.0.2 打包策略定稿（2026-10-14/15 多轮 Vortex 实测后）**：**彻底放弃 FOMOD**。Vortex 1.0.2 的 fomod-installer 对 `<ModuleConfig.xml>` 有**内部自相矛盾**：`XmlScriptType.GetXmlScriptVersion` 在 root 上读 `xmlScriptVersion` 属性，但同一 schema 又把该属性判 `Sch_UndeclaredAttribute`（带出 NullRef 或 `UndeclaredAttribute(Line1)`+`InvalidElementContentExpecting(installSteps)`）。试过 5 种组合（有header/去header/去header+requiredInstallFiles/根xmlScriptVersion=2.0双s/三s+before+Language源）**全部死在 installSteps 或 GetXmlScriptVersion**。**最终决策**：Vortex 与 MO2 都用"**无 FOMOD 普通 zip**"：`Data\SKSE\Plugins\{AKFS.dll,AKFS.ini}`，语言靠**两个独立 zip**（无后缀=英文默认、`_CHS`=中文）。老式 XmlScript、FOMOD v1/v2、用 FOMOD 做语言选择 **全部回退放弃**。⚠️ 代价：无安装时语言选择，改语言=换另一个 zip 重装，或手动改 ini `[Lang]Language=English/中文`。
  > **交付物**：`E:\SE mod\Another Key for Shout 1.0.2.zip`(英文默认,256057B)、`Another Key for Shout 1.0.2_CHS.zip`(中文,256082B)，均含 DLL 615936B(v1.5)+ini(UTF-8 干净,`Language` 正确区分)；`AKFS_1.0.2_MO2_FOMOD_legacy.zip`(旧 FOMOD 版留档)、`AKFS_source_1.0.2.zip`(源码)。
  > **已实测字节级验证**：zip 内 ini 为干净 UTF-8（`E9 85 8D...`="配置"，非 GBK 乱码；pwsh 终端显示乱码是代码页视觉误导，非真实编码错误）。
  > ⚠️ **FOMOD 教训(写死记住)**：ModuleConfig.xml 若用`xmlScriptVersion=/xmlSscriptVersion=`根属性→Vortex 报 `UndeclaredAttribute`；有 `installSteps`→`InvalidElementContentExpecting`；仅 moduleName+requiredInstallFiles（无 installSteps）→`GetXmlScriptVersion:NullRef`。**任何带 installSteps 的 FOMOD 在这个 Vortex 上都装不了**，别再试。
- [x] **环境工具确认**：git ✅、VS Build Tools(MSVC) ✅、VS 自带 CMake/Ninja ✅；vcpkg ❌(弃用，走 thirdparty)；Skyrim SE 安装路径 ⏳(需用户提供，调试阶段用户自己处理)。
- [~] **功能需求已确认**（X 键第二槽，**新规格见 §2.1**：魔法菜单按 X 记录高亮项，游戏内按 X 临时换槽→`AddCastPower`→还原）— ✅ 已实现并编译通过。
- [x] **重建 X 键第二槽插件(新规格)**：
  - [x] `src/Version.cpp` — `SKSEPluginInfo` 宏，导出 `SKSEPlugin_Version/Query`（SKSE 加载必需）。
  - [x] `src/MenuMouseIntercept.{h,cpp}` — 纯查询：`IsZCastForm`(Shout 恒真 / SpellType kPower/kLesserPower/kVoicePower)、`GetSpellFromForm`(龙吼取 variations[0].spell)、`ResolveHighlightedForm`(读魔法菜单高亮项，rootList+inventoryLists 双路径)。
  - [x] `src/SecondSlotManager.{h,cpp}` — X 键监听(BSTEventSink<InputEvent*>)+场景分化；`CastSecondSlot` 经 **`MagicCaster::CastSpellImmediate`** 立即施放(替代已证失败的 voiceState 直写/AddCastPower，见 §4.5)。
  - [x] **待用户实测**(魔法菜单按 X 记录 → 退出菜单按 X 是否真正施放)——✅ **2026-10-13 实测通过**，功能可用（部署: 把 `E:\SE mod\SEMod.dll` 覆盖到 `E:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\SEMod.dll`，Address Library 需装）。
- [x] 创建 CMake 工程骨架 + 引入 CommonLibSSE-NG；`build/out/Release/SEMod.dll` 编译通过，dumpbin 已验证导出 `SKSEPlugin_Load/Query/Version` ✅。
- [x] 魔法菜单 swf 读取字段名已实测定位（§2.2 第3条），无需再按 §4 猜测改字段名。后续迭代在次节「待办/待改进」里登记。
- [x] **v0.4 冷却 + 长按蓄力龙吼已实现并编译通过**（决策见 §3 2026-改进）：施放前冷却门禁 + 回写 `high->voiceRecoveryTime`；龙吼按住蓄力分级/松开施放；力量按下即施放带固定冷却。**⚠️ 用户实测后否决 v0.4 的自算蓄力**（冷却采纳，蓄力不行 — 见 §3 2026-v0.5）。
- [x] **v0.5 已成：蓄力改"原版蓄力"（完整注入原版 Z 键通道）**，冷却沿用：龙吼走 `ShoutHandler::ProcessButton` 注入 Z 键按下/释放事件让引擎原生定级+冷却；力量仍 `CastSpellImmediate`+固定冷却。已编译通过，`E:\SE mod\SEMod.dll`(583680B) 已更新。**⚠️ 用户实测否决（延迟/丢瞬时一阶段/卡住），见 §3 2026-v0.6**。
- [x] **v0.6 已成：回退到自算蓄力 + CastSpellImmediate**（弃用 v0.5 注入通道与 currentShout 改写，见 §3 2026-v0.6）：龙吼按下记时、松开按阈值选级(瞬时点按落 1 级)取 `variations[level].spell` 经 CastSpellImmediate 施放 + 回写原版冷却；力量不变。`E:\SE mod\SEMod.dll`(584704B) 已更新。**⚠️ 用户实测否决（无吼声/抖动），见 §3 2026-v0.7**。
- [x] **v0.7 已成：龙吼施放原语改回 `Actor::AddCastPower(SpellItem*)`（引擎原版 Shout 通道，拿回吼声/抖动）**，自算蓄力选级与冷却门禁不变、力量不变。`E:\SE mod\SEMod.dll`(584704B) 已更新。
- [x] **v0.10 待用户实测（延迟还原 currentShout）**——✅ **2026-10-13 实测通过**：蓄力分级/切换龙吼均正常。用户另报两点（开关效应 + 吼声是 Z 槽的）在 v0.11/v0.12 处理（见 §3）。
- [x] **v0.15 用户 2026-10-13 实测**：**"z选择的龙吼不会乱换了"（选槽来回跳 ✅ 修复）**；但**仍报两点残留**：
  ①"仍然需要按一次 x 来取消红字状态"——X 施放后龙吼菜单见红字(使用中)，需再按一次 X 才解除；
  ②"短按的第一阶段是 z键的龙吼的第一阶段的声音，但是第二,三阶段没有这个问题"。
  用户确认红字**与冷却无关**（冷却中依然可换选龙吼）→ 否决"voiceRecoveryTime 残留"假设（§四之九原 issue-1 提示证伪）。
- [x] **v0.17 修复版用户实测（2026-10-13）**：红字已解决 ✅（voiceState 归零才还原 currentShout 生效）；但 **1/2 阶声音仍错**——1 阶=Z 槽 1 阶声；2 阶=X 槽蓄力声后接 Z 槽 2 阶声；3 阶正确。`currentShout` 全程钉在第二槽(00013E07)却**仍听到 Z 声** → 证明 **异步施放(1/2 阶)的声音来源是 `selectedPower`(0x1E0) 而非 currentShout**（v0.15/v0.17 已把 selectedPower"立即还原"回 Z 槽 00070980）。
- [x] **v0.18 修复版实测（2026-10-13）**：**龙吼全阶正确 ✅**（声音全程第二槽，红字不回归）。
- [x] **v0.19 特殊能力修复版已交付**（能力独立冷却 + 施放能力前清残留；编译导出通过，待用户实测）。
- [ ] **v1.4 只对 kPower 套"当日一次" + DailyBypass 后门待用户实测**（`E:\SE mod\AKFS_latest_for_game.dll`(613888B,2026/10/14 16:36) 覆盖游戏 Plugins\AKFS.dll，**重开游戏**）：
  > **现状**：仅 `SpellType==kPower` 且不在 `[Settings] DailyBypass` 后门清单的能力才套当日门禁；kLesserPower/kVoicePower 只走 3s 短冷却。
  > **UESP 依据**（Skyrim:Powers 页，来源 URL 见 §3 v1.4）：Greater Powers(大能力,=kPower) 每天一次；Lesser Powers(=kLesserPower) 可任意次；Beast Form+净化 Ring of Hircine 是"kPower 但可反复"的异常 → DailyBypass 后门覆盖。
  > **待测**：①高贵血统(kPower) 当日第一次正常施放、同日第二次被拦、隔日解禁；②吸血鬼领主变身(kLesserPower) 当日可反复使用(只受 3s 冷却)；③在 ini 加 `DailyBypass=00092C48`（Beast Form）后，狼人变身当日内可反复施放。
- [ ] **v1.3 特殊能力"一天一次"待用户实测**（`E:\SE mod\AKFS_latest_for_game.dll`(610304B,2026/10/14 16:07) 覆盖游戏 Plugins\AKFS.dll，**重开游戏**）：
  > **探针实测结论(2026-10-14 16:02日志, 高贵血统 formID=000E40C8)**: 连续 4 次按 X 施放, `[power-probe]` IsInCastPowerList **恒为 0**（含施放后）→ 实锤 **CastSpellImmediate 不写引擎 castPowerList**，引擎的"当天已用"标记对我们的施放路径**不可见/不可用**。原版确有每天一次机制(castPowerList + kPowerUsed)，但仅原版 AddCastPower/CheckCast 通道会写它，而 AddCastPower 已在 AE 实测不可靠(v0.7/v2.0)。
  > **方案(已实现)**: 用 `Calendar::GetDaysPassed()`(游戏日取整) 自行按 formID 记当日施放 → `OnCooldown` 能力分支先查 `PowerUsedToday(fid)`（开启时当日二次 X 被拦）；ini `[Settings] PowerOncePerDay=1/0`(默认 1)。施放后 `MarkPowerUsedToday` 写当前游戏日（跨日取整变化时清旧重记，保证每游戏日一次零残留）。3s 独立冷却 m_powerCdUntil 保留（每日一次是叠加在短冷却之上的更严限制）。
  > **待测**: ①当日第一次按 X 施放高贵血统正常；②同游戏日内第二次按 X 不施放(无特效)；③睡到次日/跨日后可再次施放；④改 `PowerOncePerDay=0` 后同日可反复施放(只受 3s 冷却)。
- [ ] **v0.20 收藏菜单选槽版待用户实测**（`E:\SE mod\SEMod_latest_for_game.dll` 覆盖游戏 Plugins\SEMod.dll，**重开游戏**）：
  ① Q 收藏菜单里按 X → 应像普通龙吼/特殊能力一样"已存入第二槽"（读取 FavoritesMenu 高亮项存槽）；
  ② 退出收藏菜单后按 X → 应施放第二槽，**不再**退出瞬间误释放；
  ③ 回传 SEMod.log 核对 `[favorites]` 前缀日志（root.selectedIndex / 命中 FormID）确认收藏项读取得当。
- [ ] **v1.1 四大 bug 修复版待用户实测**（`E:\SE mod\AKFS_latest_for_game.dll` → 游戏 `Data\SKSE\Plugins\AKFS.dll`，**重开游戏**）：
  ① 进游戏后**第一次按 X** 施放龙吼 → 应**不再直接三阶**，按真实按住时长定级（净化 `currentShoutVariation/voiceTimeElapsed` 生效）；
  ② 首次施放龙吼后 → **Z 键管理的龙吼不应变成 X 槽龙吼**（Z 槽保持原选）；
  ③ 魔法菜单存第二槽 → **退出重进游戏**后第二槽标记仍在（AKFS.ini `[Settings] SecondSlot` 持久化）；
  ④ X 施放特定力量 → 冷却中再按 X 该力量**被冷却拦截**，但**换另个力量施放不受阻**（per-formID 独立冷却）。
  回传 `Another Key for Shout.log` 核对 `[begin]` 净化日志（currentShoutVariation=0）、`(已持久化)` 、`[power]` formID 冷却日志。
- [ ] **v1.2 待用户实测**（`E:\SE mod\AKFS_latest_for_game.dll`(605184B) → 游戏 `Data\SKSE\Plugins\AKFS.dll`，**重开游戏**）：本次专测**"设 X 槽后第一次按 X"**场景——
  ① **设 X 槽**(魔法菜单按 X 存)后**第一次按 X**(短按、正常松手) → 应**不再直接三阶**，按真实按住时长出 1 阶（`FeedNativeHeld` 物理松手检测生效，up 被吞也立即结算）；
  ② 该次施放后 **Z 键龙吼不应变成 X 槽**（进龙吼菜单看当前选中仍是你 Z 选的——`RestoreCurrentShout` 无条件还原 selectedPower 生效）；
  ③ 回传 `Another Key for Shout.log` 核对 `[restore] selectedPower 还原前 当前=... m_previousPower=...`（还原值应是玩家 Z 选，非 X 槽）。

---

## 6. 记忆维护约定

- 每次做完一个可交付步骤，同步更新「当前状态」「待办」「决定日志」三处。
- 记忆尽量短、具体、可检索；大段原文/代码放对应源文件，只在记忆里写结论与指针。
- 有新的坑就加进「教训」表，标注解决方式，避免二次踩坑。

---

## 7. 环境快照 (探测于搭建前)

| 工具 | 状态 | 位置 / 版本 |
|------|------|--------------|
| git | ✅ 可用 | git 2.51.1.windows.1 |
| VS Build Tools | ✅ 已装 | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` — 实为 channel **VisualStudio.18.Release**(VS Build Tools 18.9.2) |
| MSVC 工具集 | ✅ 可用 | 最新 **14.51.36231**(另有 14.44.35207)；`cl.exe` Hostx64\x64 存在 |
| vcvars64 | ✅ | `VC\Auxiliary\Build\vcvars64.bat` |
| CMake | ✅ (VS 自带) | cmake **4.3.1-msvc1**，在 BuildTools `Common7\IDE\...\CMake\bin` |
| Ninja | ✅ (VS 自带) | BuildTools `Common7\IDE\...\CMake\Ninja` |
| vcpkg | ❌ 未装 | 无 `VCPKG_ROOT`；C:\vcpkg、~/vcpkg、E:\vcpkg 均无 |
| SkyrimSE 1.6.1170 | ⚠️ 未定位 | 默认 Steam 路径 & E:\Steam 都没找到 → **需用户提供安装路径** |
| winget | ✅ | 可用 |
| choco / scoop | ❌ | 无 |

> ⚠️ 注意事项：MSVC 工具集 **14.51 偏新**。CommonLibSSE-NG 官方多在各 Release 验证，较新编译器偶发 ABI/编译告警差异；若 CMake 引入后编译报错，可回退到工具集 14.44 或用构建时指定版本(记录于 CLAUDE 审查章节补充)。
>
> ✅ **已实测打通(最终)**：`Visual Studio 17 2022` generator + `-A x64 -T v145`，CommonLibSSE-NG 3.6.0 配置通过、编译通过；插件 `SEMod.dll` 编译成功（`build/out/Release/SEMod.dll`，导出 `SKSEPlugin_Load` 已验证）。依赖走 git clone 到 `thirdparty/` + spdlog 预 install(脚本见 `scripts/build_spdlog.ps1`)。命令参考：
> ```bat
> cmake -S E:\SE mod -B E:\SE mod\build -G "Visual Studio 17 2022" -A x64 -T v145
> cmake --build E:\SE mod\build --config Release
> ```
> 注：需在 vcvars64 环境或 VS 开发者命令行下运行(因 CMake 用 MSVC cl.exe)；若在普通 pwsh，cmake 需完整路径(见 §4)。

---

## 8. 构建产物与运行定位

- **插件 DLL**：`E:\SE mod\build\out\Release\AKFS.dll`（文件名已由 `SEMod.dll` 改名，见决定日志 2026-改名）
- **运行部署**：把该 DLL 复制到游戏 `Data\SKSE\Plugins\AKFS.dll`（= 文件名与 DLL 一致）下；需已安装 **Address Library for SKSE Plugins**（SE 1.6.x）与对应游戏 runtime (1.6.1170)。当前部署副本 `E:\SE mod\AKFS_latest_for_game.dll`。
- **运行部署**：把该 DLL 复制到游戏 `Data\SKSE\Plugins\` 下；需已安装 **Address Library for SKSE Plugins**（SE 1.6.x）与对应游戏 runtime (1.6.1170)。
- **📦 发布 zip（1.0.2 起，无 FOMOD 普通包）**：`Another Key for Shout 1.0.2.zip`(英文默认/无后缀)、`Another Key for Shout 1.0.2_CHS.zip`(中文/有后缀)。内部结构 `Data\SKSE\Plugins\{AKFS.dll,AKFS.ini}`。MO2/Vortex 均可直接装（无 FOMOD 脚本最兼容）。旧 FOMOD 版存档 `AKFS_1.0.2_MO2_FOMOD_legacy.zip`。
- **日志文件**：由 `SKSE::log` 输出到 `My Games\Skyrim Special Edition\SKSE\SEMod.log`（首次加载时创建）。
- **调试责任**：本项目调试/实测阶段由**用户**负责；此工作区只交付可编译的 DLL 骨架与构建环境。
