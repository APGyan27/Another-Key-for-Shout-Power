# Knowledge/INPUT_FUNCTION_CHAIN.md — 输入按键→龙吼/力量施放 函数链 (实测级)

> 用途：作为"按 X 无反应"问题的权威判定依据。来源 = 阅读 CommonLibSSE-NG 源码
> (E:\SE mod\thirdparty\CommonLibSSE-NG)。所有地址均经 Address Library RELOCATION_ID
> 运行期解析，不写死游戏内部地址。

## 一、按键输入→动作分发的真实链条

```
设备(键盘/鼠标/手柄)
  └─ BSInputDeviceManager::PollInputDevices(float)   RELOCATION_ID(67315, 68617)
        - 对每个 device 调 Process()
        - 调 ControlMap::sub_140C11600(InputEvent*)   ← 按键→userEvent("Shout"/"XButton"...)
        - 调 Rumble::Update
        - Emits the last InputEvent                    ← 经 BSTEventSource<InputEvent*> 发布
        - 重置全局 BSInputEventQueue
        └─ BSTEventSource<InputEvent*>::SendEvent(const Event* a_event)
              - 遍历注册的 sinks，调每个 sink 的 ProcessEvent(a_event, this)
              - 返回 kStop 则中断后续 sink
```

- `BSInputDeviceManager : BSTEventSource<InputEvent*> (偏移 00) + BSTSingletonSDM (偏移 58)`。
  sink 列表在 base 源对象(偏移 0) `sinks`。`AddEventSink(this)` 把插件 sink 写入该列表。
- `SendEvent(const Event*)` 中 `Event = InputEvent*`，对每个 sink 调 `ProcessEvent(a_event, this)`。
- 插件把 `SecondSlotManager`(继承 `BSTEventSink<InputEvent*>`) AddEventSink 到该源，理论上会被调用。

## 二、已确认的关键类 / VTable

- `BSTEventSource<Event>`：`sinks` 偏移00 / `pendingRegisters` 18 / `pendingUnregisters` 30 /
  `lock` 48 / `notifying` 50；`AddEventSink/RemoveEventSink/SendEvent`。
- `BSTEventSink<Event>` vtable：dtor@00, `ProcessEvent(Event const*, BSTEventSource<Event>*)@01`。
- `PlayerControls : BSTEventSink<InputEvent*>(000)+... + BSTSingletonSDM，持有 handlers[] 数组
  (成员 0x58)，含 `ShoutHandler* shoutHandler`(成员 0x1B0)。游戏内部经此 sink 分发按键到各 handler。
- `PlayerInputHandler` vtable：dtor@00, `CanProcess@01`, `ProcessThumbstick@02`, `ProcessMouseMove@03`,
  `ProcessButton@04`；`inputEventHandlingEnabled` 成员 0x08。
- `ShoutHandler : PlayerInputHandler`：override `CanProcess(InputEvent*)@01`、
  `ProcessButton(ButtonEvent*, PlayerControlsData*)@04` —— 这是原版 Z 键龙吼施放的**真正入口**。
- `UserEvents`(单例) 持有全部按键动作名 BSFixedString；`xButton` 成员 @0x300 = "XButton"。
- `ControlMap : BSTEventSource<UserEventEnabled>` —— 只发布"控制被启用/禁用"通知(UserEventEnabled
  仅含 new/old UEFlag)，**不发布按键动作**；按键映射在 PollInputDevices 内 sub_140C11600 完成。

## 三、施放路径

- **`MagicCaster::CastSpellImmediate(MagicItem*, bool noHitFxArt, TESObjectREFR* target,
  float effectiveness, bool hostileOnly, float magnitudeOverride, Actor* blameActor)`** ——
  MagicCaster vtable 槽 01，是 engine 最终"立即执行一个 spell"的唯一原语，可靠性最高。
  经 `Actor::GetMagicCaster(MagicSystem::CastingSource::kInstant)` 取得玩家 instant 施放器后调用。
  `MagicSystem::CastingSource::kInstant=3`。
- 施放目标表单：`TESShout::variations[VariationID::kOne].spell`(每变体含 `SpellItem*`)；
  力量类 SpellItem 直接施放(SpellType kPower/kLesserPower/kVoicePower)。
- `Actor::AddCastPower(SpellItem*)` RELOCATION_ID(37787, 38736) —— 原版 Z 键的另一个施放函数，
  EE 瞬发式不写 caster currentSpell、不改 selectedPower/currentShout。
- **❌ 已实测失败：直接写 `HighProcessData::voiceState`(=kRequest) + `currentShout` 不会触发引擎
  真正施放** —— 游戏内人物完全不播吼动作(用户 2026-10-12 实测，见 SEMod.log 有"施放请求已提交"但
  "人物没有吼/播动作")。教训：不要直接改 voice 数据结构期望引擎处理，必须走真正施放原语。
- 施放目标槽(供参考)：`Actor::ACTOR_RUNTIME_DATA::selectedPower`(TESForm*, 0x1E0) 力量槽；
  `HighProcessData::currentShout`(TESShout*, 0x008) 龙吼槽——`currentProcess→high` 取。

## 四、诊断核心疑问 (由探针日志判定)

插件挂 `BSInputDeviceManager`(BSTEventSource<InputEvent*>) `AddEventSink` 后，`ProcessEvent` 是否被调用？
- 若日志**出现** `SEMod[probe]: ProcessEvent hit` → 事件到达，问题在按键匹配或施放路径。
- 若日志**没有**任何 probe 行 → 该游戏版本对该源不向第三方 sink 发布(或 PowerControls 已消费/
  内部只走特殊路径)，需改挂更可靠接入点(实例：hook `PlayerControls::ProcessEvent` vtable 或
  `ShoutHandler::ProcessButton`)。

> 注意：`BSInputDeviceManager` 设备层事件是 ControlMap 映射**之前**的原始设备事件，覆盖所有
> 上下文(含魔法菜单打开)，这也正是本插件选它作为接入点的原因——菜单内也要能按 X 存槽。

## 四之二、v0.4 — 冷却 + 长按蓄力 (2026-10-13)

- **冷却门禁**：`CastSpellImmediate` 是瞬发原语，**跳过引擎 voice 冷却** → 可无限连发。v0.4 改为
  施放前读 `Actor::GetVoiceRecoveryTime()`(返回 `currentProcess->high->voiceRecoveryTime`)当门禁>0拒绝；
  施放后**回写** `high->voiceRecoveryTime`=该级 `recoveryTime`。0x018 字段只被引擎当"剩余冷却"读取
  (含"声音未恢复"UI 提示、`CombatInventoryItemShout` 评估)，写它是安全的(不像 voiceState 会被引擎拿来
  驱动动画)。龙吼冷却=对应级 `variations[level].recoveryTime`；力量=固定 `kPowerRecoveryTime`。
- **长按蓄力龙吼**：X 按下只记时蓄力(`m_charging`/`m_chargeStart`)，松开按时长选等级
  (阈值 1.0s→Level2 / 2.5s→Level3)取 `variations[level].spell` 经 `CastSpellImmediate` 施放。
  按下/释放沿用 `m_wasDown` 状态变化判定(BSInputDeviceManager 键盘按下/释放各回调一次)。
- **关键字段实测地址**：`HighProcessData::voiceState(0x000)/currentShout(0x008)/currentShoutVariation(0x010)/
  voiceTimeElapsed(0x014)/voiceRecoveryTime(0x018)/voiceTimer(0x328)`；
  `Actor::ACTOR_RUNTIME_DATA::currentProcess(AIProcess*, 0x0F0)` → `.high`。
- **⚠️ 编译坑**：`RE::DebugNotification` 声明在 `RE/M/Misc.h`；`currentProcess` 不是 Actor 直接成员，
  须经 `GetActorRuntimeData().currentProcess` 取。
- **⚠️ 用户实测否决**：v0.4 自算蓄力（按下记时→松开选级）**用户判定"蓄力不行"**——默认按下即触发逻辑，
  且未用原版蓄力；自算 1 级阈值 1.0s 对快速点按(实测仅 ~19ms)直接落 Level1，快速点按与实际需求不符。
  → 冷却方案采纳，蓄力改**原版蓄力**（见四之三）。

## 四之三、v0.5 — 原版蓄力（完整注入原版 Z 键通道）(2026-10-13)

- **决策**：经 `ask_user_question`，用户确认 **Z 键为默认布局（物理码 `REX::W32::DIK_Z`=0x2C）**，并选
  **完整注入原版 Z 键通道**。目标：让引擎自己处理蓄力（音效/升级/动画/冷却全原生），不自算等级。
- **实现（龙吼）**：按下沿 `StartNativeShoutCharge()`：
  1. `player->GetActorRuntimeData().currentProcess->high->currentShout` 临时换成第二槽龙吼，存原值 `m_previousShout`；
  2. 构造 Z 键按下事件 `ButtonEvent::Create(INPUT_DEVICE::kKeyboard, UserEvents::shout, DIK_Z, 1.0F, 0.0F)`
     （value=1.0,held=0 → `IsDown()==true`）；
  3. `PlayerControls::GetSingleton()->shoutHandler->ProcessButton(evt, &pc->data)` → 引擎进入原版蓄力。
  释放沿 `FinishNativeShoutCharge()`：注入 `ButtonEvent::Create(..., DIK_Z, 0.0F, heldSecs)`
  （value=0.0,held>0 → `IsUp()`，heldSecs=真实按住秒数供引擎定级）→ 引擎按蓄力时长分级施放并自动设原版冷却；
  随后还原 `high->currentShout = m_previousShout`。
- **关键 API**：`ButtonEvent::Create(INPUT_DEVICE, BSFixedString, uint32_t idCode, float value, float heldDownSecs)`
  在 CommonLib `RE/B/ButtonEvent.h`（使用引擎 `malloc`）。`UserEvents::GetSingleton()->shout`="Shout"。
  `ShoutHandler : PlayerInputHandler` 的 `ProcessButton(ButtonEvent*, PlayerControlsData*)` 是原版 Z 键入口，
  vtable 槽位 @04（`RE/S/ShoutHandler.h`）；`PlayerControls` 的 `shoutHandler`(0x1B0)/`data`(0x24)，静态 `GetSingleton()`。
- **力量（非龙吼）**：不蓄力（原版如此），按下沿直接 `CastSpellImmediate`(kInstant) 立即施放 + `SetVoiceCooldown(kPowerRecoveryTime=3.0s)`。
- **冷却门禁**：沿用 v0.4 `OnCooldown()`——`Actor::GetVoiceRecoveryTime()>0` 则拒绝（也防与引擎原版冷却冲突）。
- **⚠️ 风险与回退**：`ShoutHandler::ProcessButton` 是引擎内部入口（CommonLib 无 .cpp 实现），AE 上是否真正进入原版
  蓄力表现未知，需实测。若注入不触发，回退：改经原版用户事件流入——构造 `ButtonEvent` 用 `UserEvents::shout` 名 +
  `InputEvent::Create` 广播回 `BSInputDeviceManager` 让原版按键分发接手（等用户日志确认）。
- **⚠️ 编译坑**：`SecondSlotManager.h` 成员 `m_secondSlot{TESForm*}` 需前向声明 `class TESForm;`（否则
  `RE::TESForm` 未定义 C2039）；`Player()` 须限定 `const`（`OnCooldown() const` 内部调用它）。

## 四之四、v0.6 — ⛔ v0.5 注入通道否决，回退自算蓄力 (2026-10-13)

- **用户实测 v0.5 三项回归**：**①延迟**（每次按 X 都有延迟）；**②丢失瞬时按键触发一阶段**（快速点按不再即时出 1 级龙吼）；
  **③有时卡住**——释放不出来的同时提示"使用龙吼"，导致原生 Z 键也无法使用其他龙吼。
- **根因判定**：`ButtonEvent::Create` + `ShoutHandler::ProcessButton` 手工注入**绕过引擎真实的按→持→放状态机**
  （单一 down 事件 + 人为 heldDownSecs 无法复原真实按住流）；且 `high->currentShout` 交换在注入失败/异常路径
  **残留** → 引擎卡在原版蓄力/龙吼占用态，后续 Z 键施放被吞。**注入通道在 AE(1.6.1170) 不可靠，判定弃用**。
- **回退决策**：**零引擎 voice 状态改写**（不碰 `high->currentShout`/`voiceState`，不注入任何 Z/按钮事件），
  回到 v0.4 自算蓄力（用户已认可其"冷却做得好"）+ `CastSpellImmediate`（唯一已证可靠原语，v0.3 实测通过）。
  - 龙吼：按下 `StartShoutCharge()` 只记 `m_shoutActive/m_shoutStart`；松开 `FinishShoutCharge()` 按
    `kChargeLevel2At=1.0s/kChargeLevel3At=2.5s` 选级（**瞬时点按落 1 级**），取 `TESShout::variations[level].spell`
    经 `CastSpellImmediate` 立即施放，回写原版冷却 `variations[level].recoveryTime`。
  - 力量：不变，按下即 `CastSpellImmediate` + `SetVoiceCooldown(kPowerRecoveryTime=3.0s)`。
  - 删除成员 `m_previousShout` 与 `Start/FinishNativeShoutCharge`、`ButtonEvent::Create`/`PlayerControls`/`ShoutHandler`。
- **字段确认（CommonLib `RE/T/TESShout.h`）**：`struct Variation{ SpellItem* spell@08; float recoveryTime@10; }`，
  `Variation variations[VariationIDs::kTotal]@60`（kTotal=3）。选级后下标用 `variations[level]`，level 越界防御 clamp。
- **教训（写入 MEMORY 决策表）**：引擎的发声施放对外不可侵入错误——**宁可用自算蓄力降级到 1/2/3 级，也不手工注入
  引擎按钮通道**（后者会破坏原生 Z 键）。自算蓄力是当前 AE 上唯一稳定可得"分级 + 瞬时一阶段 + 冷却"的方案。

## 四之五、v0.7 — 龙吼改用引擎原版施放函数拿回吼声/抖动 (2026-10-13)

- **v0.6 再被用户否决**：自算蓄力 + `CastSpellImmediate` 修好了延迟/卡住，但**无吼声/无抖动**（"感觉空"）。
  `CastSpellImmediate` 只触发法术特效/音效，绕过引擎 Shout 发声处理（吼叫音效 + 屏幕抖动）。
- **用户判断**："改进 0.5 版本，感觉是因为抢了什么函数导致的" → 指向 v0.5 注入抢了引擎的函数流/currentShout。
- **解法**：保持 v0.6 自算蓄力选级（瞬时 1 级、无注入、无延迟、不卡），把龙吼施放原语改回
  **`Actor::AddCastPower(SpellItem*)`**（RE/A/Actor.h line 499）——它是 **Z 键原版施放入口**：
  - 走引擎 **Shout 通道** → 得到**原版吼声 + 相机抖动**（这正是 CastSpellImmediate 缺失的）；
  - v0.2 首次实现即用此函数、实测**施放可靠**；
  - 实测**不写 `caster.currentSpell` / `selectedPower` / `high->currentShout`**（MEMORY §2）→ 零状态污染、
    不抢函数、不卡原生龙吼（规避 v0.5 的"抢+残留"）。
- **实现**：`FinishShoutCharge()` 中 `caster->CastSpellImmediate(...)` 替换为 `player->AddCastPower(spell)`；
  自算选级、`SetVoiceCooldown(variation.recoveryTime)`、力量分支(仍 CastSpellImmediate) 均不变。
- **依赖**：无新增 include（`Player->AddCastPower` 来自已含的 RE/A/Actor.h，Player() 返回 PlayerCharacter* 派生自 Actor）。

## 四之九、v0.9→v0.15 — 引擎 Shout 通道最终形态 (2026-10-13)

> 背景：v0.7(AddCastPower) 在 AE 只弹提示不真施放；v0.8 回到 `ShoutHandler::ProcessButton` 注入 Z 键，
> 每帧喂 held 流修"点按不出一级"，但 held 期间引擎把 `currentShout` 换回选中槽→施放成 Z 槽；v0.9 全程钉住
> `currentShout`；v0.10 延迟还原(一二阶异步结算) 修"只剩三阶"。v0.14 换 selectedPower 修"Z 选能力时 X 只出
> 能力音效、放不出龙吼 + 吼声"；v0.15 修"X 施放后 menu 变红 / 需多按一次 X / Z 选槽来回跳"。

- **⚠️ 实测定论（v0.11→v0.15）**：
  - **绝不写 `high->voiceState`**。v0.11/v0.12 复位 voiceState=kNone → 施放后卡在动作/拔不出来（恶性）。
  - **`selectedPower`(0x1E0) 决定 voice 通道分流"龙吼/能力"**——施放期必须临时换成第二槽龙吼才进得了
    龙吼通道并拿对吼声（v0.14）。但它是**菜单/HUD"当前选中/使用中"身份**：施放 up 后必须**立即**还原回
    玩家 Z 选龙吼，不能走延迟——否则开龙吼菜单被当"使用中"变红（Z 施放不动 selectedPower → 不变红，
    这正是 X/Z 行为差异根源）。
- **最终形态（v0.15）**：施放期 `currentShout` + `selectedPower` 都换成第二槽龙吼；`FinishNativeShout`
  喂 up 后 **selectedPower 立即还原**（m_previousPower 即时清零），`currentShout` **延迟 800ms 还原**
  （异步早阶结算要读）；`RestoreCurrentShout` **只还原 currentShout**（不再碰 selectedPower，否则用已清空
  的 m_previousPower=null 清掉玩家 Z 选龙吼 → 来回跳）。绝不写 voiceState。
- **字段/类型确认（CommonLib `RE/H/HighProcessData.h`、`RE/A/Actor.h`）**：`VOICE_STATE`(\@0x000,
  只读不写)；`currentShout`(TESShout*, 0x008)；`currentShoutVariation`(0x010)；`selectedPower`(TESForm*,
  0x1E0，voice 分流 + 菜单选中身份)。`voiceRecoveryTime` 为冷却（施放后由引擎/SetVoiceCooldown 设置，
  若残留也会致"变红"，需与 selectedPower 区分）。
- **⚠️ issue-1 实锤（v0.16 日志，2026-10-13）**：红字**与 voiceRecoveryTime 冷却、selectedPower 均无关**。
  根因是 **1/2 阶异步结算读 currentShout 时它已被固定 800ms 还原成 0/Z槽** → 播 Z 声 + voice 状态卡住不归零
  → 菜单持续"施放中"红字（"再按一次 X"= 二次完整 down→up 才终结状态机）。**v0.17 修复**：延迟还原改为
  "voiceState 归零才还原（3s 硬上限）+ pending 每帧重钉 currentShout=第二槽"。
- **⚠️ issue-2 实锤（v0.16 日志）：1/2 阶(异步)播 Z 槽声音、3 阶(同步)正确**——同步阶 up 当帧 voiceState
  已 kPostcast 施放完成（currentShout 未还原）→ 声音对；异步阶 up 后 voiceState 才 kContinue/kStart，
  冻结结算跨帧，读到已还原 currentShout → Z 声。v0.17 条件还原让异步结算也吃到第二槽。
- **⚠️ issue-3 实锤（v0.18 日志，2026-10-13）：1/2 阶(异步)的声音来源是 `selectedPower`(0x1E0)，不是 currentShout**。
  v0.17 的 currentShout 全程钉在第二槽(00013E07)但用户仍听到 Z 声 → 异步施放的"哪个龙吼在发声"由
  **selectedPower** 决定。而 v0.15/v0.17 已把 selectedPower **立即还原**回玩家 Z 槽(00070980) →
  异步结算读 selectedPower=Z 槽 → 播 Z 声。现象完全吻合：1 阶=Z 槽 1 阶声；2 阶=先 X 蓄力声(hold 期
  currentShout=第二槽给的蓄力音效)再 Z 槽 2 阶声(异步施放读回 selectedPower=Z 槽)；3 阶同步在 up 帧内
  selectedPower 尚=第二槽 → X 声。
- **⚠️ issue-4 实锤（v0.17）：红字与 selectedPower 无关**——v0.15 立即还原 selectedPower 仍红字；v0.17
  只改 currentShout 还原时机（voiceState 归零才还原）即修好红字。**红字由 currentShout/voiceState 决定**。
- **最终形态（v0.18）**：`selectedPower` **不再立即还原**，与 `currentShout` 一起纳入**条件还原**——
  `TryDeferredRestore` 每帧把两字段都重钉回第二槽，等 voiceState 归零(kNone=0)或 3s 硬上限才一起还原；
  `RestoreCurrentShout` 扩展为同时还原 currentShout+selectedPower（仅当 `m_previousPower` 非空才写
  selectedPower，避免把玩家既有 Z 选槽清成 null）。约定：**换槽暂存方法**——`BeginNativeShout` 里先存
  `m_previousShout=currentShout`、`m_previousPower=selectedPower` 再换，还原时依此回位。
- **字段/上述判定依据（SEMod.log 2026-10-13）**：3 阶 up 后 `voiceState=5(kPostcast) variation=2(kThree)`；1 阶 up 后 `voiceState=2(kStart)/3(kContinue) variation=0(kOne)`；空闲 voiceState=0(kNone)。`voiceState` 值域：kNone=0,kRequest=1,kStart=2,kContinue=3,kPrecast=4,kPostcast=5,kFail=6。