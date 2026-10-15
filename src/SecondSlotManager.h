// SecondSlotManager.h — X 键第二槽管理器
// v1.1：修复三处 bug（基于用户实测 2026-10-14 反馈）：
//   - 修复"首次进游戏按 X 龙吼直接出第三阶段"：进游戏时 high->currentShoutVariation 是脏值(-1)，
//     注入原版蓄力前先净化(归零)该变体字段 + 清空 voiceTimeElapsed，让引擎蓄力从干净状态开始。
//   - 修复"Z 槽被 X 龙吼污染"：BeginNativeShout 记录并正确还原玩家原 currentShout / selectedPower
//     （Z 未选龙吼时 currentShout=null、选中能力时 selectedPower=能力原槽），施放后无条件还原回原值。
//   - 修复"能力无视自身冷却"：能力冷却改为**按各能力 formID 独立计时**（能力间不共享冷却），
//     施放判断尊重该能力自己的冷却。
// v0.20：收藏菜单也能按 X 存槽。
// v0.19：能力冷却独立 + 施放能力前清残留龙吼延迟还原。
// v0.18：selectedPower 也条件还原 — 修 1/2 阶声音
// v0.16 加了只读快照诊断；v0.17 据日志实锤把"固定 800ms 还原 currentShout"改为
//   "voiceState 归零才还原（3s 硬上限）"，使 1/2 阶异步结算吃到第二槽 → 声音对、voice 状态自然归零。
// v0.18 据 2026-10-13 SEMod.log 实锤：引擎**异步施放(1/2 阶)的声音来源是 selectedPower(0x1E0)
//   而非 currentShout**（3 阶同步在 up 帧内 selectedPower=第二槽→X 声；1/2 阶异步跨帧施放时
//   selectedPower 已被 v0.15 方案"立即还原"回玩家 Z 槽→播 Z 声；2 阶"先吐 X 槽蓄力声(hold 期
//   currentShout=第二槽) 再吐 Z 槽二阶段声(异步施放读回 selectedPower=Z 槽)"完全符合此类）。
//   红字无关 selectedPower（v0.15 立即还原 selectedPower 仍红字；v0.17 只改 currentShout 还原
//   时机即修好红字→红字由 currentShout/voiceState 决定）。故本版 selectedPower **不再立即还原**，
//   与 currentShout 一样纳入**条件还原**（voiceState 归零才还原，重钉也两字段一起重钉），保证
//   1/2 阶异步结算全程读到第二槽 → 声音全程 X 槽。
// v0.15：currentShout 延迟还原、selectedPower 立即还原 — 解耦）
//
// 职责：监听键盘 X 键，按魔法菜单开关分化：
//   - 菜单开着：把鼠标高亮的龙吼/力量存入 m_secondSlot；
//   - 菜单关着：按 X 施放第二槽（龙吼=原版蓄力+吼声+抖动；力量=立即施放+冷却）。
//
// v0.9 修复 v0.8 的两项用户回归（换 currentShout+喂 Z 键事件，每帧 held + 全程钉住 currentShout）。
// v0.10 修复"只出三阶段、一二阶卡住"：一二阶异步结算 → currentShout 延迟还原 800ms，由 TryDeferredRestore 兜底。
// v0.13/v0.14：selectedPower(0x1E0) 决定 voice 通道分流"龙吼/能力"，施放期需临时换成第二槽龙吼才进得了龙吼通道
//   并拿对吼声（只换 currentShout，Z 选能力时按 X 只出能力音效放不出龙吼）。
// v0.15 修复"X 施放后 menu 变红 + 需多按一次 X 解除 + Z 选槽来回跳"：
//   - 根因 A（变红）：selectedPower 是菜单/HUD"当前选中/使用中"身份。施放 up 后延迟还原期间仍=第二槽，
//     开龙吼菜单被当"使用中"→ 变红；原版 Z 施放不动 selectedPower → 不变红，此为 X/Z 行为差异根源。
//   - 根因 B（来回跳）：延迟还原兜底 TryDeferredRestore → RestoreCurrentShout 用已清零的 m_previousPower=null
//     再写 rt.selectedPower，清掉玩家 Z 选龙吼；且菜单按 X 的 StoreHighlightedToSlot 会重存槽，交错后来回跳。
//   - 修法：解耦 —— selectedPower 施放 up 后**立即**还原回玩家 Z 选龙吼（m_previousPower 即时清零）；
//     currentShout 仍延迟 800ms 还原（异步早阶结算要读它）。
// ⚠️ 实测铁律：绝不写 high->voiceState（v0.11/v0.12 卡动作真凶）。
//
// 冷却门禁：施放前读 Actor::GetVoiceRecoveryTime()（>0 拒绝）；施放后由引擎原版设置冷却（并回写
// voiceRecoveryTime 兜底），引擎 UI/原版 Z 键/下次 X 都尊重。
//
// 输入走 BSInputDeviceManager 的 BSTEventSink<InputEvent*>。不挂任何 detour，对原生输入零干扰。
#pragma once

#include "SKSE/Impl/PCH.h"
#include "RE/B/BSTEvent.h"
#include "RE/I/InputEvent.h"

#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace RE
{
    class PlayerCharacter;
    class TESForm;
    class TESShout;
    class SpellItem;
}

namespace SEMod
{
    class SecondSlotManager final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        // 单例访问。生命周期与插件共存，用静态实例最安全。
        [[nodiscard]] static SecondSlotManager& GetSingleton();

        ~SecondSlotManager() override = default;

        // 挂到 BSInputDeviceManager 事件源。应在 kDataLoaded 后调用。
        void RegisterInputSink();

        // 清空已存槽位。
        void ClearSlot();

    private:
        SecondSlotManager() = default;

        // BSTEventSink<InputEvent*> 的 ProcessEvent。形参是 INPUT_DEVICE 指针指针。
        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_event,
            RE::BSTEventSource<RE::InputEvent*>* a_source) override;

        // 魔法菜单已打开：把当前高亮且属于 Z 键类（龙吼 / Power 类法术）的表单存入 m_secondSlot。
        void StoreHighlightedToSlot();

        // X 按下（菜单关、非冷却）：龙吼 → BeginNativeShout（原版蓄力/吼声/抖动）；
        // 力量 → CastPowerNow（立即施放 + 固定冷却）。
        void OnXDown();

        // 冷却门禁：声音是否未恢复。⚠️ v1.1 按槽类型分流——龙吼走引擎 voice 冷却；
        // 能力走**按能力 formID 独立冷却**（能力间互不共享，见 OnCooldown / CastPowerNow）。
        bool OnCooldown() const;

        // —— 龙吼：引擎原版 Shout 通道（v0.8，修复 v0.5"抢函数"根因）——
        // 按下：临时把 high->currentShout 换成第二槽龙吼，喂 Z 键 down 事件进 ShoutHandler，
        // 引擎进入原版蓄力（吼声/升级/抖动全原生）。
        void BeginNativeShout();
        // 按住期间每帧喂 Z 键 held 事件（heldDownSecs=真实累计按住时长）——让引擎蓄力计时
        // 按真实时长推进（v0.5 失败就在于只喂单次 down，没有按帧 held 流，改不生引擎蓄力态）。
        void FeedNativeHeld();
        // 释放：喂 Z 键 up 事件（引擎按蓄力时长分级施放并设原版冷却），随后还原 currentShout。
        void FinishNativeShout();
        // 立即施放力量/能力（CastSpellImmediate）并设固定冷却。
        void CastPowerNow();
        // 只读判断：X 槽当前存的是不是龙吼（TESShout）。非空返回 true（能力/其他法术= false）。
        [[nodiscard]] bool IsSlotShout() const;

        // ⚠️ v1.2：Win32 直接查询触发键当前是否仍按下（GetAsyncKeyState，DIK 扫描码转 VK）。
        // 修复"X 键 up 事件偶尔丢失 → 游戏认为没松手 → 蓄力无限累计到三阶"的根因：
        // 不再依赖可能丢失的 up 事件，而是每帧轮询物理键状态判定真实松开。
        [[nodiscard]] bool IsXKeyPressed() const;

        // 还原被临时换下的 currentShout（释放后 / 异常退出都归位，避免残留占用原生龙吼）。
        void RestoreCurrentShout();
        // 延迟还原检查：松开 up 后 currentShout 需在第二槽停留一小段时间，让引擎异步结算早阶(1/2)
        // 施放（三阶同步结算、早阶异步→若立即还原会施放成 Z 槽/卡住）。到期或新流程开始前调用。
        void TryDeferredRestore();

        // 只读判断：作为"每天一次"门禁——该能力在 **当前游戏日** 是否已施放过（防止单日重复）。
        // 游戏日取 Calendar::GetDaysPassed()（float 游戏天数）。探针实锤 CastSpellImmediate 不写
        // 引擎 castPowerList(IsInCastPowerList 恒 0)，故自行按日跟踪。返回 true=当日已用过。
        [[nodiscard]] bool PowerUsedToday(std::uint32_t a_formId) const;
        // 记录该能力当日已施放（存当前游戏日到 m_powerUsedDay[formID]）。
        void MarkPowerUsedToday(std::uint32_t a_formId);
        // 只读判断：该能力是否应套"当日一次"门禁 —— 仅 SpellType==kPower 且不在 DailyBypass 后门清单。
        // （kLesserPower/kVoicePower 等可反复用的能力永不套当日限制，只走 3s 短冷却。）
        [[nodiscard]] bool IsDailyPower(const RE::SpellItem* a_spell) const;

        // 玩家引用缓存（避免多次 GetSingleton）。
        RE::PlayerCharacter* Player() const;

        RE::TESForm*   m_secondSlot{ nullptr };   // 第二槽，可存 TESShout* 或 SpellItem*（仅 Z 键类）
        bool           m_shoutActive{ false };    // 当前是否正注入原版蓄力（Z 键 held 流中）
        bool           m_wasDown{ false };        // 上一事件 X 是否按下，用于检测沿
        std::chrono::steady_clock::time_point m_shoutStart{};  // 注入按下时点
        RE::TESShout*  m_previousShout{ nullptr };              // 被临时换下的原 currentShout
        RE::TESForm*   m_previousPower{ nullptr };              // 被临时换下的原 selectedPower(0x1E0, 决定 voice 通道 shout/能力)
        bool           m_restorePending{ false };              // 是否正等待延迟还原 currentShout
        std::chrono::steady_clock::time_point m_restoreAt{};   // 延迟还原到点时刻
        // v1.1：能力独立冷却——按各能力 formID 记最近施放时刻（能力间不共享冷却）。
        std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> m_powerCdUntil;
        // v1.3：能力"一天一次"——按各能力 formID 记最近施放时的**游戏日**(Calendar::GetDaysPassed 取整)。
        std::unordered_map<std::uint32_t, float> m_powerUsedDay;
        bool           m_storePendingRelease{ false };         // v0.20 本次 X 是在菜单里用于存槽——抬起时不施放（防"退出菜单即释放"）
    };
}