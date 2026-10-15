// SecondSlotManager.cpp — v0.20 收藏菜单选槽（基于用户 2026-10-13 需求）。
//   用户需求：在"喜欢菜单"(=按 Q 弹出的 FavoritesMenu 侧菜单)里也能像普通龙吼/特殊能力一样
//   按 X 选中存第二槽（此前只识别 MagicMenu → Q 菜单里按 X 走施放路径，无效果；退出菜单还误释放）。
//   实现：
//   ① 选槽菜单识别从 MagicMenu 扩展到 **MagicMenu 或 FavoritesMenu**；
//   ② FavoritesMenu 的收藏项直接存在 C++ 侧 rt.favorites(BSTArray<Entry>，Entry.item=TESForm*)，
//      高亮索引从 swf root.selectedIndex 读，越界/读不到兜底取"第一个 Z 键类收藏项"
//      （见 MenuMouseIntercept::ResolveFavoritesHighlightedForm）；比对 MagicMenu 的 swf 读取可靠。
//   ③ 引入 m_storePendingRelease：在菜单里按 X 存槽的这一次，**抬起 X 时不施放**
//      （防"退出菜单瞬间 X 抬起被当施放"）。
// v0.19 特殊能力修复（基于用户 2026-10-13 实测反馈）。
//   用户确认：龙吼全阶正确（v0.18 ✅）。剩余两个特殊能力问题：
//   ① Z=龙吼、X=能力时，龙吼冷却中 X 能力放不出 —— 根因：冷却只走引擎 voice 冷却
//     (GetVoiceRecoveryTime) 单一门槛，龙吼施放占用了它 → X 能力被当年 voice 冷却拦截。
//     修复：冷却按【槽类型】分流 —— 能力槽走插件独立计时 m_powerBlockUntil，
//     与 voice 冷却完全解耦，龙吼冷却不再挡能力；同时能力冷却也不回写 voiceRecoveryTime，
//     避免反过来污染龙吼冷却。龙吼槽仍走引擎 voice 冷却（与 Z 键一致）。
//   ② 吸血鬼领主下 X→恢复形态(Revert Form) 无效；随后正常选恢复形态并按 Z → 崩溃。
//     崩栈(2026-10-13 19:59)在 armor/BipedAnim attach 路径（hdtsmp/hair 等第三方 mod 参与），
//     是变身(transform)状态切换 + 铠甲重挂载时的交错。本插件视角的隐患：能力施放可能
//     残留上一轮龙吼的延迟还原(restorePending currentShout/selectedPower 操纵)，在能力施放/
//     变身状态下继续操纵槽位 → 与游戏 transform 清除交错。修复：CastPowerNow 施放前
//     先清掉任何残留 restorePending（RestoreCurrentShout 归位），能力施放本身不碰
//     currentShout/selectedPower/voiceState，天然干净。
// v0.18 修复版（基于 v0.16/v0.17 诊断日志的实锤根因）。
//   ⚠️ 本版把 v0.17 的"currentShout 条件还原"扩展到 **selectedPower 一并条件还原**：
//     1/2 阶声音仍错（1 阶=Z 槽 1 阶声；2 阶=X 槽蓄力声后接 Z 槽 2 阶声）的根因（2026-10-13
//     SEMod.log 实锤）：引擎**异步施放(1/2 阶)的声音来源是 selectedPower(0x1E0) 而非 currentShout**。
//     - 3 阶: up 当帧同步结算，selectedPower 尚=第二槽 → 读到的就是第二槽 → X 声 ✅。
//     - 1 阶: 异步跨帧施放，v0.15/0.17 已把 selectedPower **立即还原**回玩家 Z 槽(00070980)
//       → 异步施放读 selectedPower=Z 槽 → 播 Z 槽 1 阶声 ✗。
//     - 2 阶: 蓄力声(hold 期 currentShout=第二槽)取当前正在蓄力的龙吼→X 声；随后的 2 阶施放声
//       异步读 selectedPower=已还原的 Z 槽→Z 声 ✗（先 X 后 Z 的现象完全吻合）。
//     - 红字无关 selectedPower（v0.15 立即还原 selectedPower 仍红字；v0.17 只改 currentShout
//       还原时机即修好红字 → 红字由 currentShout/voiceState 决定）。
//     修复：selectedPower 不再立即还原，与 currentShout 一起纳入**条件还原**——HTTPs pending 每帧
//     两字段都重钉回第二槽，等 voiceState 归零(或 3s 硬上限)才一起还原 → 1/2 阶异步结算全程读到
//     第二槽 → 声音全程 X 槽。红字保持不回归（还原时机判据未变）。
//   保留 v0.16 只读快照诊断日志。绝不写 voiceState。
// v0.16 诊断版（v0.15 逻辑不变，仅新增只读快照诊断日志）。
//   目标：定位两个残留症状——①X 施放后龙吼菜单见"使用中"红字，需再按一次 X 才解除；
//   ②短按(1 阶)播的是 Z 槽龙吼的 1 阶声音，2/3 阶正确。
//   诊断点：BeginNativeShout(新流程前)、FinishNativeShout(up 前后)、TryDeferredRestore(还原后)，
//   各打印只读 voiceState/currentShoutVariation/currentShout/selectedPower/voiceRecoveryTime，
//   用于判定红字与 1 阶声音到底由哪个 voice 身份字段驱动。绝不写 voiceState。
// v0.15 实现（currentShout 延迟还原 + selectedPower 立即还原解耦；
// 修复"X 施放后 menu 变红 + 需再按一次 X 解除 + Z 选槽来回跳"）。
// 根因：selectedPower 是菜单/HUD"当前选中/使用中"身份，施放 up 后须**立即**还原回玩家 Z 选龙吼，
//   否则开菜单被当"使用中"变红（Z 施放不动 selectedPower → 行为差异）；且延迟还原兜底若再写
//   m_previousPower(已清零→null) 会清掉玩家 Z 选龙吼 → 来回跳。故 selectedPower 即时还原、currentShout
//   仍延迟 800ms（异步早阶结算）。绝不写 voiceState。）
#include "SecondSlotManager.h"

#include "AKFSConfig.h"
#include "MenuMouseIntercept.h"
#include "SKSE/Impl/PCH.h"
#include "SKSE/Logger.h"
#include "RE/A/AIProcess.h"
#include "RE/A/Actor.h"
#include "RE/B/BSInputDeviceManager.h"
#include "RE/B/ButtonEvent.h"
#include "RE/C/Calendar.h"
#include "RE/F/FormTraits.h"
#include "RE/F/FavoritesMenu.h"
#include "RE/H/HighProcessData.h"
#include "RE/M/MagicCaster.h"
#include "RE/M/MagicMenu.h"
#include "RE/M/MagicSystem.h"
#include "RE/M/Misc.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/P/PlayerControls.h"
#include "RE/S/ShoutHandler.h"
#include "RE/S/SpellItem.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESShout.h"
#include "RE/U/UI.h"
#include "RE/U/UserEvents.h"
#include "REX/W32/DINPUT.h"

// ⚠️ v1.2：为 IsXKeyPressed 查询物理键状态，需 Win32 GetAsyncKeyState/MapVirtualKeyW。
// 必须放在所有 RE/CommonLib 头之后（最后一个 RE 头 DINPUT.h 之后），避免 windows.h 的
// macro(GetObject/min/max/small 等)污染 CommonLib 头导致编译错（见 MEMORY 教训）。
#include "windows.h"

namespace SEMod
{
    namespace
    {
        constexpr float kPowerRecoveryTime = 3.0F;              // 力量固定冷却（秒）
        constexpr auto  kShoutRestoreMinHold = std::chrono::milliseconds(300);   // pending 最短保持（防过早归零处理）
        constexpr auto  kMaxShoutRestoreWait = std::chrono::milliseconds(3000);  // pending 硬上限（voiceState 万一不归零的兜底）

        // 按 [Lang] 语言出提示：中文 / English（Fallback：读不到配置则中文）。
        void NotifyLocalized(const char* a_cn, const char* a_en)
        {
            if (AKFSConfig::English())
                RE::DebugNotification(a_en);
            else
                RE::DebugNotification(a_cn);
        }
    }

    SecondSlotManager& SecondSlotManager::GetSingleton()
    {
        static SecondSlotManager instance;
        return instance;
    }

    RE::PlayerCharacter* SecondSlotManager::Player() const
    {
        return RE::PlayerCharacter::GetSingleton();
    }

    void SecondSlotManager::RegisterInputSink()
    {
        // 首次触发时读取 AKFS.ini 配置（键位 + 语言）。
        const auto& ini = AKFSConfig::Load();
        SKSE::log::info("AKFS: 读取配置 {}，触发键 DIK={}，语言={}",
                        ini, static_cast<int>(AKFSConfig::KeyCode()),
                        AKFSConfig::English() ? "English" : "中文");

        auto* inputMgr = RE::BSInputDeviceManager::GetSingleton();
        if (!inputMgr) {
            SKSE::log::error("AKFS: BSInputDeviceManager singleton 为空，无法注册输入 sink");
            return;
        }
        inputMgr->AddEventSink(this);
        SKSE::log::info("AKFS: SecondSlotManager 输入 sink 已注册");

        // ⚠️ v1.1 问题 3 修复：加载持久化的第二槽（AKFS.ini 的 [Settings] SecondSlot）。
        // 游戏数据已就绪(kDataLoaded)，可用 TESForm::LookupByID 还原出 TESShout/SpellItem。
        const std::uint32_t saved = AKFSConfig::SecondSlotFormID();
        if (saved != 0) {
            RE::TESForm* form = RE::TESForm::LookupByID(saved);
            if (form && MenuMouseIntercept::IsZCastForm(form)) {
                m_secondSlot = form;
                SKSE::log::info("AKFS: 已从配置恢复第二槽 FormID={:08X}（{}）",
                                saved, form->GetName());
            } else {
                SKSE::log::warn("AKFS: 配置里的第二槽 FormID={:08X} 无法解析为龙吼/力量，忽略", saved);
            }
        }
    }

    void SecondSlotManager::ClearSlot()
    {
        m_secondSlot = nullptr;
        m_shoutActive = false;
        m_wasDown = false;
        m_shoutStart = {};
        m_restorePending = false;
        m_restoreAt = {};
        m_storePendingRelease = false;
    }

    // ⚠️ v1.2：查询触发键（默认 X）当前物理上是否仍被按下。
    // 修复"X 键 up 事件偶尔丢失 → 游戏认为没松手 → 蓄力无限累计到三阶"的根因。
    // 不用 CommonLib 的 BSWin32KeyboardDevice::IsPressed（其虚函数实现未随 CLIBS 链接，
    // 引用会导致 LNK2001），改用 Win32 GetAsyncKeyState：
    //   - 入参是 DIK 扫描码（AKFSConfig::KeyCode()），先用 MapVirtualKeyW 转成虚拟键码 VK；
    //   - GetAsyncKeyState 高位(0x8000)置位 = 该键此刻仍被按下（游戏在前台时可靠）。
    bool SecondSlotManager::IsXKeyPressed() const
    {
        const auto vk = static_cast<UINT>(::MapVirtualKeyW(AKFSConfig::KeyCode(), MAPVK_VSC_TO_VK));
        if (vk == 0)
            return false;  // 扫描码无效：保守按"已松开"处理（由原事件逻辑兜底）。
        return (::GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
    }

    RE::BSEventNotifyControl SecondSlotManager::ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>* a_source)
    {
        // 每帧任意输入事件都先处理延迟还原，保证 currentShout 到期后尽快归位。
        TryDeferredRestore();

        // v0.20：选槽菜单 = 魔法菜单(MagicMenu) 或 收藏菜单(FavoritesMenu, Q 键侧菜单)，
        // 二者打开时按 X 都是"选中存槽"语义（不施放）。
        const bool inStoreMenu = RE::UI::GetSingleton()->IsMenuOpen(RE::MagicMenu::MENU_NAME)
                                 || RE::UI::GetSingleton()->IsMenuOpen(RE::FavoritesMenu::MENU_NAME);

        // ⚠️ v1.2 兜底（问题 "首次 X 直出三阶" 的真正根因）：X 键的 up 事件偶尔丢失，
        // 导致 m_wasDown 恒为 true、游戏误以为玩家一直按住 → 松手后蓄力仍按墙上时钟累计，
        // 一下子冲到三阶。这里每帧（任意输入事件进入本函数时）直接查询物理 X 键：
        // 只要 m_wasDown 正在蓄力但物理键已松开，就立即结算 FinishNativeShout，
        // 不再等那个可能丢失的 up 事件。非菜单、非存槽场景才生效。
        if (m_wasDown && m_shoutActive && !IsXKeyPressed()) {
            if (!m_storePendingRelease && !inStoreMenu) {
                m_wasDown = false;
                FinishNativeShout();
            }
        }

        auto* ev = a_event ? *a_event : nullptr;
        for (auto* it = ev; it; it = it->next) {
            if (it->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
                continue;

            auto* button = it->AsButtonEvent();
            if (!button)
                continue;

            // 触发键：默认物理 X 键，可用 AKFS.ini 的 [Settings] KeyCode 改（DIK 扫描码）。
            const bool isX = button->GetIDCode() == AKFSConfig::KeyCode();
            if (!isX)
                continue;

            const bool down = button->IsDown();
            const bool held = button->IsHeld();   // 按住期间 BSInputDeviceManager 每帧发 held 事件
            const bool up = button->IsUp();

            // ⚠️ [DIAG v1.2.1] 决定性探针：dump 每次收到的 X 键事件原始形态 + 内部状态。
            // 定位"设槽后第一次按 X 出三阶"到底是不是 up 事件没到/被吞、heldSecs 多少、
            // m_wasDown 是否卡在 true。每次 X 事件都打，串起来看完整时序。
            SKSE::log::info(
                "AKFS: [diag-x] down={} held={} up={} heldSecs={:.3f} | m_wasDown={} m_shoutActive={} storePendingRelease={} inStoreMenu={}",
                down ? 1 : 0, held ? 1 : 0, up ? 1 : 0,
                button->HeldDuration(),
                m_wasDown ? 1 : 0, m_shoutActive ? 1 : 0,
                m_storePendingRelease ? 1 : 0, inStoreMenu ? 1 : 0);

            // 按下沿：按下瞬间（heldDownSecs==0 的 down 事件）。
            if (down && !m_wasDown) {
                m_wasDown = true;
                if (inStoreMenu) {
                    // 菜单里按下 = 存槽，并记录"本次 X 抬起不施放"，防止退出菜单瞬间被误判施放。
                    m_storePendingRelease = true;
                    StoreHighlightedToSlot();
                } else {
                    // 非菜单 = 一次真实按键，立即清掉上一次存槽遗留的"抬起不施放"标志，
                    // 确保本次抬起正常结算施放（否则 m_shoutActive 会泄漏，见 up 分支）。
                    m_storePendingRelease = false;
                    if (!OnCooldown())
                        OnXDown();
                    // 冷却中：静默（不提示），直接忽略本次 X。
                }
            }
            // 按住期间：BSInputDeviceManager 每帧对按住键发 held 事件。
            // 若正注入原版龙吼蓄力，就把每帧 held 时长转发给引擎 Shout 状态机
            // （像真实 Z 键）让蓄力计时按真实时长推进 —— 修复 v0.5"只喂单次 down"改不生蓄力态的根因。
            else if (held && m_wasDown) {
                if (!inStoreMenu && m_shoutActive)
                    FeedNativeHeld();
            }
            // 释放沿：由按下转为抬起。
            else if (up && m_wasDown) {
                m_wasDown = false;
                // 存槽场景（且本段**未**开始真实施放）的抬起：不施放，仅清存槽标志。
                // ⚠️ 必须带 !m_shoutActive：设槽后第一次按 X 若已开始真实施放（m_shoutActive=true），
                // up 绝不能因 storePendingRelease 被吞——否则 m_shoutActive 永远不归 0，
                // 下一次 X 走 held 的墙钟累计，引擎按超长蓄力直出三阶（实测 16.8s→三阶）。
                if (m_storePendingRelease && !m_shoutActive) {
                    m_storePendingRelease = false;
                    break;
                }
                if (!inStoreMenu) {
                    if (m_storePendingRelease)   // 进行中的真实施放：清掉残留标志，正常结算。
                        m_storePendingRelease = false;
                    FinishNativeShout();
                }
            }
            break;
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    // X 按下（菜单关、非冷却）：龙吼 → 引擎原版蓄力；力量 → 立即施放 + 固定冷却。
    void SecondSlotManager::OnXDown()
    {
        if (!m_secondSlot) {
            NotifyLocalized("AKFS: 尚未存入第二槽（魔法菜单中按 X 记录）", "AKFS: No second slot stored (press X in the magic/favorites menu to set)");
            return;
        }
        if (m_secondSlot->As<RE::TESShout>()) {
            BeginNativeShout();
            return;
        }

        CastPowerNow();
    }

    void SecondSlotManager::StoreHighlightedToSlot()
    {
        SKSE::log::info("AKFS: 菜单 X 键 → 尝试存槽");
        auto* ui = RE::UI::GetSingleton();
        // ⚠️ v0.20：优先读魔法菜单（Tab 魔法界面）；若未开则在收藏菜单(Q 键侧菜单)里读高亮项。
        RE::TESForm* form = nullptr;
        if (ui && ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
            form = MenuMouseIntercept::ResolveHighlightedForm();
        } else {
            form = MenuMouseIntercept::ResolveFavoritesHighlightedForm();
        }
        if (!form) {
            SKSE::log::info("AKFS: 解析菜单高亮项返回空，无法存槽");
            NotifyLocalized("AKFS: 无法读取菜单当前高亮项", "AKFS: Could not read the highlighted menu item");
            return;
        }
        if (!MenuMouseIntercept::IsZCastForm(form)) {
            SKSE::log::info("AKFS: 高亮项 FormID={:08X} 非龙吼/力量，跳过", form->GetFormID());
            NotifyLocalized("AKFS: 该项不是龙吼/力量，跳过存槽", "AKFS: Not a shout/power, slot not changed");
            return;
        }
        m_secondSlot = form;
        m_shoutActive = false;
        m_wasDown = false;
        // ⚠️ v1.1 问题 3 修复：存槽即写回 AKFS.ini，退出游戏重进后第二槽仍保留。
        AKFSConfig::SetSecondSlot(form->GetFormID());
        SKSE::log::info("AKFS: 已记录第二槽 FormID={:08X}（已持久化）", form->GetFormID());
        NotifyLocalized("AKFS: 已存入第二槽", "AKFS: Second slot set");
    }

    // ⚠️ v1.1 冷却按【槽类型 + 槽身份】分流（问题 4 修复）：
    //   - 第二槽=龙吼：用引擎 GetVoiceRecoveryTime()（原版 voice 冷却，与 Z 键一致）。
    //   - 第二槽=能力：用插件独立计时 m_powerCdUntil，**按各能力 formID 分别记冷却**
    //     （能力与龙吼不共享冷却；能力之间也不共享冷却——施放能力 A 后冷却中，施放能力 B 不受阻）。
    bool SecondSlotManager::OnCooldown() const
    {
        auto* player = Player();
        if (!player)
            return false;
        if (m_secondSlot && m_secondSlot->As<RE::TESShout>()) {
            const float remain = player->GetVoiceRecoveryTime();
            if (remain > 0.0F) {
                // ⚠️ v1.5 残留自愈：龙吼施放后引擎 voiceRecoveryTime 正常恒为 0（18:55 全程实测）。
                //   "用久了"无法释放 = 某次异步施放让引擎残留 voiceRecoveryTime>0 且未归零 → 恒拦。
                //   引擎的 voice 冷却只在它自己施放通道才推进/归零；我们的 X 施放(CastSpellImmediate/
                //   BeginNativeShout 注入)引擎并不主动清它。故只要插件**当前不处于施放龙吼中**
                //   (m_shoutActive/m_restorePending 均 false)，残留的 voice 冷却就是无效垃圾。
                //   此时允许施放并把残留归零，避免"永久锁死"。绝不写 voiceState（铁律），只清 recoveryTime。
                if (!m_shoutActive && !m_restorePending) {
                    SKSE::log::warn("AKFS: [cd] 龙吼槽 voiceRecoveryTime 残留 {:.3f}s 且插件空闲，判定为垃圾冷却，归零放行",
                                    remain);
                    // 直接清 high->voiceRecoveryTime（无 Set 方法；它只是 HighProcessData 成员 0x018）。
                    // ⚠️ 只清 recoveryTime，绝不写 voiceState（铁律）。
                    auto& prt = player->GetActorRuntimeData();
                    if (prt.currentProcess && prt.currentProcess->high)
                        prt.currentProcess->high->voiceRecoveryTime = 0.0F;
                    return false;
                }
                // 插件自身仍在施放龙吼（pending 未还原）：真冷却，拦截。
                SKSE::log::info("AKFS: [cd] 龙吼槽被 voice 冷却拦截 remain={:.3f}s", remain);
                return true;
            }
            return false;
        }
        // 能力槽：先查"一天一次"门禁（**仅 kPower 且非 DailyBypass 后门**，v1.4；默认开）；
        // 再查该能力的独立冷却（与 voice 冷却及别的能力均解耦）。
        if (m_secondSlot) {
            const std::uint32_t fid = m_secondSlot->GetFormID();
            // 当日一次：仅对"应套当日限制"的能力判定——kPower 且不在后门清单方可查 PowerUsedToday。
            if (AKFSConfig::PowerOncePerDay() && IsDailyPower(m_secondSlot->As<RE::SpellItem>()) && PowerUsedToday(fid)) {
                SKSE::log::info("AKFS: [cd] 能力槽 {:08X} 被当日一次拦截", fid);
                return true;
            }
            const auto it = m_powerCdUntil.find(fid);
            if (it != m_powerCdUntil.end() && std::chrono::steady_clock::now() < it->second) {
                SKSE::log::info("AKFS: [cd] 能力槽 {:08X} 被短冷却拦截 (剩余={:.2f}s)", fid,
                                std::chrono::duration<float>(it->second - std::chrono::steady_clock::now()).count());
                return true;
            }
        }
        return false;
    }

    // 该能力在"当前游戏日"是否已施放过。游戏日取 Calendar::GetDaysPassed() 的整数部分。
    // ⚠️ v1.3.1 修复"隔了一天仍无法触发"：必须**比较存储的游戏日 == 当前游戏日**，而不是只看条目存在。
    //   （原实现 `find!=end` 即判已用，但跨日清理只发生在成功施放路径 → 一旦被拦就永远拦死。）
    //   当日条目 → 已用；游戏日已变(跨日) → 视为未用。只读，不改状态。
    bool SecondSlotManager::PowerUsedToday(const std::uint32_t a_formId) const
    {
        const auto it = m_powerUsedDay.find(a_formId);
        if (it == m_powerUsedDay.end())
            return false;
        auto* cal = RE::Calendar::GetSingleton();
        const float day = cal ? cal->GetDaysPassed() : 0.0F;
        // 比较"游戏日"而非精确 float：同一天内存的条目 days==floor(今天)，隔天后 days < 今天 → 解禁。
        return static_cast<std::int64_t>(it->second) == static_cast<std::int64_t>(day);
    }

    // 记录该能力当日已施放：存下当前游戏日。跨日(重进/睡到隔天)后 GetDaysPassed 取整变化，
    // 由施放路径在写前先清理过期条目（见 CastPowerNow），保证"每游戏日一次"零残留。
    void SecondSlotManager::MarkPowerUsedToday(const std::uint32_t a_formId)
    {
        auto* cal = RE::Calendar::GetSingleton();
        const float day = cal ? cal->GetDaysPassed() : 0.0F;
        m_powerUsedDay[a_formId] = day;
    }

    // 该能力是否套"当日一次"门禁。规则：
    //   - 仅当 SpellType == kPower(2)（原版大能力=每天一次，UESP 明文）；kLesserPower(3)/kVoicePower(11)
    //     等可反复用的能力**永不套当日限制**，只走 3s 短冷却。
    //   - 命中 ini `[Settings] DailyBypass` 后门清单的 kPower 能力**豁免**当日限制
    //     （应对"类型是 kPower 但你可反复用"的例外组合，如 Beast Form + 净化 Ring of Hircine 可反复变身）。
    bool SecondSlotManager::IsDailyPower(const RE::SpellItem* a_spell) const
    {
        if (!a_spell)
            return false;
        if (a_spell->GetSpellType() != RE::MagicSystem::SpellType::kPower)
            return false;
        if (AKFSConfig::DailyBypassContains(a_spell->GetFormID())) {
            SKSE::log::info("AKFS: [daily] 能力 '{}' 命中 DailyBypass 后门，豁免当日一次", a_spell->GetName());
            return false;
        }
        return true;
    }

    // 只读判断：X 槽当前存的是龙吼还是其他。
    // 非空 → 龙吼（TESShout）；空或能力/法术 → false。不改任何状态。
    bool SecondSlotManager::IsSlotShout() const
    {
        return m_secondSlot && m_secondSlot->As<RE::TESShout>() != nullptr;
    }

    // —— 龙吼：引擎原版 Shout 通道（v0.9 修复"施放错龙吼 + 点按不出一级"）——
    // 用户关键记忆："v0.5 里可以选择其他龙吼" → 换 high->currentShout + 喂 Z 键事件这条路是通的。
    // v0.8 回归根因：每帧 FeedNativeHeld 期间引擎会重新解析当前龙吼，把 high->currentShout 换回
    // 玩家实际选中的 Z 槽龙吼 → 松开施放时就成了 Z 槽龙吼。修复：在整个蓄力期间（含每帧 held 与释放
    // up 前）持续把 currentShout 钉在第二槽龙吼上，并加日志确认引擎到底读到哪个龙吼。
    void SecondSlotManager::BeginNativeShout()
    {
        // 新流程开始前，先消化上一轮未还原的 currentShout（避免跨次 X 残留）。
        TryDeferredRestore();

        if (!m_secondSlot)
            return;

        auto* shout = m_secondSlot->As<RE::TESShout>();
        auto* player = Player();
        if (!shout || !player)
            return;

        auto& rt = player->GetActorRuntimeData();
        if (!rt.currentProcess || !rt.currentProcess->high) {
            SKSE::log::error("AKFS: 玩家 currentProcess/high 为空，无法进入原版蓄力");
            return;
        }
        RE::HighProcessData* high = rt.currentProcess->high;

        // ⚠️ v1.1 首词净化（问题 1 修复）：进游戏首次施放时 high->currentShoutVariation 是脏值
        // （=VariationIDs::kNone 即 -1，见实测日志 4294967295），voiceTimeElapsed 也可能残留，
        // 若带着这些脏状态注入 Z 键蓄力，引擎会把第一声直接判到最高阶段（三阶）。
        // 这里在注入前把它们复位为干净起点（kOne=0 表示按最低级开始蓄力），
        // 让引擎按真实的按住时长重新定级。⚠️ 只动变体/计时字段，绝不写 voiceState（铁律）。
        high->currentShoutVariation = RE::TESShout::VariationIDs::kOne;
        high->voiceTimeElapsed = 0.0F;

        // 记录原 currentShout 并把第二槽龙吼钉上去（蓄力全程保持）。
        m_previousShout = high->currentShout;
        high->currentShout = shout;
        SKSE::log::info("AKFS: [begin] 原 currentShout={:08X} → 换为第二槽={:08X}",
                        m_previousShout ? m_previousShout->GetFormID() : 0,
                        shout->GetFormID());

        // 只读诊断：新蓄力流程开始前引擎 voice 状态（判断上一次施放是否留下残留）。
        SKSE::log::info("AKFS: [begin] 前快照 voiceState={} currentShoutVariation={} voiceRecoveryTime={:.2f}s",
                        static_cast<std::uint32_t>(high->voiceState.underlying()),
                        static_cast<std::uint32_t>(high->currentShoutVariation),
                        high->voiceRecoveryTime);

        // ⚠️ 关键（v0.14）：selectedPower(0x1E0) 决定引擎 voice 通道走"龙吼还是能力"。
        // 实测 v0.13 只换 currentShout 时，若 Z 选的是特殊能力，按 X 只出能力音效、放不出龙吼——
        // 引擎根本没进龙吼通道。故必须把 selectedPower 也临时换成第二槽龙吼，才能进龙吼通道并拿对吼声。
        // 与 currentShout 同步换、同步延迟还原（不碰 voiceState，v0.11 卡动作是 voiceState kNone 所致，
        // 与 selectedPower 换槽无关——用户 2026-10-13 定论）。
        m_previousPower = rt.selectedPower;
        rt.selectedPower = shout;
        SKSE::log::info("AKFS: [begin] 原 selectedPower={:08X} → 换为第二槽={:08X}",
                        m_previousPower ? m_previousPower->GetFormID() : 0,
                        shout->GetFormID());

        auto* pc = RE::PlayerControls::GetSingleton();
        if (!pc || !pc->shoutHandler) {
            high->currentShout = m_previousShout;
            m_previousShout = nullptr;
            rt.selectedPower = m_previousPower;
            m_previousPower = nullptr;
            SKSE::log::error("AKFS: PlayerControls/shoutHandler 为空，无法注入 Z 键蓄力");
            return;
        }

        // 喂 Z 键 down（value=1.0, held=0 → IsDown()），触发引擎 ShoutHandler 开始蓄力。
        RE::ButtonEvent* downEv = RE::ButtonEvent::Create(
            RE::INPUT_DEVICE::kKeyboard,
            RE::UserEvents::GetSingleton()->shout,
            REX::W32::DIK_Z,   // 默认 Z 键物理码
            1.0F, 0.0F);
        m_shoutActive = true;
        m_shoutStart = std::chrono::steady_clock::now();
        pc->shoutHandler->ProcessButton(downEv, &pc->data);

        SKSE::log::info("AKFS: 已注入 Z 键 down，引擎原版蓄力 '{}'", shout->GetName());
    }

    // 按住期间每帧：先重新把 currentShout 钉在第二槽龙吼（防御引擎 held 期间把它换回玩家选中槽），
    // 再喂 Z 键 held 事件（heldDownSecs=真实累计按住时长）让引擎蓄力计时按真实时长推进并刷吼声/升级。
    void SecondSlotManager::FeedNativeHeld()
    {
        if (!m_shoutActive)
            return;

        // ⚠️ v1.2 加固：蓄力期每帧查询物理触发键。X 的 up 事件偶尔会被引擎吞掉（尤其
        // "设 X 槽后第一次按 X"），导致 m_wasDown 恒 true、蓄力按墙上时钟无限累计到三阶。
        // FeedNativeHeld 是蓄力期间每帧必被调用的可靠挂载点——此处检测物理键已松开即立即结算，
        // 不再依赖可能丢失的 up 事件。FinishNativeShout 内部由 m_shoutActive 防重复执行。
        if (!IsXKeyPressed()) {
            FinishNativeShout();
            return;
        }

        auto* player = Player();
        if (!player)
            return;
        auto& rt = player->GetActorRuntimeData();

        // 引擎在每帧 held 事件推进蓄力时可能重读并换回玩家选中的 Z 槽龙吼；
        // 这里每帧都钉回第二槽，保证松开时引擎解析到的是第二槽。日志只在异变时打，避免刷屏。
        RE::HighProcessData* high = rt.currentProcess ? rt.currentProcess->high : nullptr;
        auto* desired = m_secondSlot ? m_secondSlot->As<RE::TESShout>() : nullptr;
        if (high && desired && high->currentShout != desired) {
            high->currentShout = desired;
            SKSE::log::warn("AKFS: [held] currentShout 被引擎换回 {:08X}，已重新钉回第二槽 {:08X}",
                            high->currentShout ? high->currentShout->GetFormID() : 0,
                            desired->GetFormID());
        }

        // 同理钉回 selectedPower：引擎 voice 通道跟随它，若被换回玩家选中项则无法进龙吼通道。
        if (desired && rt.selectedPower != desired) {
            rt.selectedPower = desired;
            SKSE::log::warn("AKFS: [held] selectedPower 被引擎换回 {:08X}，已钉回第二槽 {:08X}",
                            rt.selectedPower ? rt.selectedPower->GetFormID() : 0,
                            desired->GetFormID());
        }

        const float heldSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_shoutStart).count();
        auto* pc = RE::PlayerControls::GetSingleton();
        if (!pc || !pc->shoutHandler)
            return;

        // value 保持 1.0，heldDownSecs 递增 → IsHeld()。
        RE::ButtonEvent* heldEv = RE::ButtonEvent::Create(
            RE::INPUT_DEVICE::kKeyboard,
            RE::UserEvents::GetSingleton()->shout,
            REX::W32::DIK_Z,
            1.0F, heldSecs);
        pc->shoutHandler->ProcessButton(heldEv, &pc->data);
    }

    // 释放：先把 currentShout 钉在第二槽（确保引擎 up 阶段的施放解析到第二槽），
    // 喂 Z 键 up（value=0.0, held>0 → IsUp()）让引擎按蓄力时长分级施放并设原版冷却，随后还原。
    void SecondSlotManager::FinishNativeShout()
    {
        TryDeferredRestore();   // 先消化上轮 pending（防御式）。
        if (!m_shoutActive)
            return;
        m_shoutActive = false;

        auto* player = Player();
        if (!player)
            return;
        auto& rt = player->GetActorRuntimeData();
        RE::HighProcessData* high = rt.currentProcess ? rt.currentProcess->high : nullptr;

        // up 前重新钉第二槽，并记录此刻引擎读到的是哪个龙吼（诊断"施放错龙吼"）。
        auto* desired = m_secondSlot ? m_secondSlot->As<RE::TESShout>() : nullptr;
        if (high && desired)
            high->currentShout = desired;
        if (high && desired)
            SKSE::log::info("AKFS: [up] 施放前 currentShout={:08X} (期望第二槽={:08X})",
                            high->currentShout ? high->currentShout->GetFormID() : 0,
                            desired->GetFormID());
        // selectedPower 同步钉到第二槽（voice 通道跟随，确保 up 阶段仍走龙吼）。
        if (desired && rt.selectedPower != desired) {
            rt.selectedPower = desired;
            SKSE::log::warn("AKFS: [up] selectedPower 已钉回第二槽 {:08X}",
                            desired->GetFormID());
        }
        SKSE::log::info("AKFS: [up] 施放前 selectedPower={:08X}",
                        rt.selectedPower ? rt.selectedPower->GetFormID() : 0);
        // 只读快照：记录 up 即将发生前引擎 voice 状态机的全部身份字段，用于判定
        // "红字/1阶声音"到底由哪个字段驱动（voiceState 绝不写，仅读诊断）。
        if (high) {
            SKSE::log::info("AKFS: [up] 前快照 voiceState={} currentShoutVariation={} voiceRecoveryTime={:.2f}s",
                            static_cast<std::uint32_t>(high->voiceState.underlying()),
                            static_cast<std::uint32_t>(high->currentShoutVariation),
                            high->voiceRecoveryTime);
        }

        const float heldSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_shoutStart).count();
        // ⚠️ [DIAG v1.2.1] 关键：up 事件到达时进入 FinishNativeShout 的真实 heldSecs（墙钟累计）。
        // 若"设槽后第一次按 X"的 heldSecs 异常偏大(几秒)，证明 up 事件确实被延迟/丢失、
        // 只能靠 FeedNativeHeld 的物理释放兜底触发 → 引擎按超长蓄力直接出三阶。
        SKSE::log::info("AKFS: [up-entry] FinishNativeShout 进入，heldSecs={:.3f} 墙钟累计(自 m_shoutStart)",
                        heldSecs);

        auto* pc = RE::PlayerControls::GetSingleton();
        if (pc && pc->shoutHandler) {
            RE::ButtonEvent* upEv = RE::ButtonEvent::Create(
                RE::INPUT_DEVICE::kKeyboard,
                RE::UserEvents::GetSingleton()->shout,
                REX::W32::DIK_Z,
                0.0F, heldSecs);   // IsUp()
            pc->shoutHandler->ProcessButton(upEv, &pc->data);
            // 诊断：up 施放后引擎实际读到的 currentShout（验证是否仍钉在第二槽），
            // 并打印 voiceState / currentShoutVariation —— 施放后若喝残留非 kNone，
            // 菜单就会把它标红（"转换中/使用中"）；这是"红字/需再按一次 X"的候选根因。
            auto* afterShout = player->GetCurrentShout();
            if (high) {
                SKSE::log::info("AKFS: [up] 施放后 快照 currentShout={:08X} voiceState={} currentShoutVariation={} voiceRecoveryTime={:.2f}s",
                                afterShout ? afterShout->GetFormID() : 0,
                                static_cast<std::uint32_t>(high->voiceState.underlying()),
                                static_cast<std::uint32_t>(high->currentShoutVariation),
                                high->voiceRecoveryTime);
            } else {
                SKSE::log::info("AKFS: [up] 施放后 currentShout={:08X} (high 为差，无法打 voice 快照)",
                                afterShout ? afterShout->GetFormID() : 0);
            }
        }

        // ⚠️ currentShout 需延迟还原（异步早阶结算要读取，见下）。
        // selectedPower 同理：日志实锤（2026-10-13 SEMod.log）——引擎**异步施放(1/2 阶)的声音来源
        // 是 selectedPower(0x1E0) 而非 currentShout**：
        //   3 阶同步在 up 帧内 selectedPower=第二槽 → X 声 ✓；1/2 阶异步跨帧施放时 selectedPower
        //   已被"立即还原"回玩家 Z 槽(00070980) → 播 Z 声 ✗。
        //   而 2 阶"先吐 X 槽蓄力声(hold 期 currentShout=第二槽) 再吐 Z 槽二阶段声(异步施放读回
        //   selectedPower=Z 槽)"完全符合此结论。
        // 红字无关 selectedPower（v0.15 立即还原 selectedPower 仍红字；v0.17 只改 currentShout
        // 还原时机即修好红字 → 红字由 currentShout/voiceState 决定）。故 selectedPower 不再立即还原，
        // 与 currentShout 一样纳入**条件还原**（等 voiceState 归零），保证异步结算读到第二槽。
        // 不再立即还原 selectedPower，m_previousPower 保留到 RestoreCurrentShout 一并还原。

        // ⚠️ 关键：不在 ProcessButton 同步调用栈里立即还原 currentShout。
        // 日志实锤（2026-10-13 SEMod.log）：三阶是**同步**结算（up 当帧 voiceState 已到 kPostcast，
        //   声音正确）；而一二阶是**异步**结算（up 后若干帧 voiceState 还停在 kContinue/kStart，
        //   施放/声音跨帧才真正完成）。若 currentShout/selectedPower 过早还原，异步结算读到的是
        //   已还原的当前槽(0/原 Z 槽) → 播 Z 槽声音 + voice 状态机卡住不归零 → 菜单持续"施放中"红字。
        // 故"延迟还原"不再按固定 800ms，而是由 TryDeferredRestore 每帧判定：
        //   ① voiceState 已归零(kNone=施放彻底完成) → 还原；
        //   ② 距 up 已满 kShoutRestoreMinHold 但 voiceState 仍未归零 → 继续挂着（让异步结算吃到第二槽），
        //      直到 voiceState 归零或碰 kMaxShoutRestoreWait 硬上限才兜底还原。
        // 用 m_restoreAt 记硬上限时刻（now+kMaxShoutRestoreWait），TryDeferredRestore 结合它判定。
        m_restorePending = true;
        m_restoreAt = std::chrono::steady_clock::now() + kMaxShoutRestoreWait;
        SKSE::log::info("AKFS: [defer-restore] 延迟还原 currentShout+selectedPower（等 voiceState 归零，最晚 {}ms）",
                        kMaxShoutRestoreWait.count());

        // ⚠️ 绝不写 voiceState。实测 v0.11/v0.12：up 后复位 high->voiceState=kNone 会与引擎 voice
        // 状态机冲突 → 施放后卡在动作/拔不出来（恶性）。selectedPower 换槽与卡无关（用户 2026-10-13 定论），
        // 只换 currentShout+selectedPower(voice 通道身份)，不碰 voiceState。
    }

    // 延迟还原检查：到期且 pending → 还原；新 X 按下/存槽前也先调用，保证不跨流程残留。
    void SecondSlotManager::TryDeferredRestore()
    {
        if (!m_restorePending)
            return;

        auto* player = Player();
        auto* high = (player && player->GetActorRuntimeData().currentProcess)
                         ? player->GetActorRuntimeData().currentProcess->high : nullptr;
        if (!player)
            return;
        auto& rt = player->GetActorRuntimeData();

        // 1) 保护窗口内不还原：up 刚发生 kShoutRestoreMinHold 之内，尽量不动，
        //    交引擎自主推进异步结算（初入 pending 的第一帧也走这里）。
        const auto now = std::chrono::steady_clock::now();
        if (now < m_restoreAt - kMaxShoutRestoreWait + kShoutRestoreMinHold)
            return;

        // 2) pending 期间每帧把 currentShout 与 selectedPower 重钉回第二槽，防止引擎异步结算步骤
        //    把两者换回玩家选中槽（若换回，异步施放/声音就读到 Z 槽）。
        //    异步施放的声音来源是 selectedPower（2026-10-13 SEMod.log 结论），所以 selectedPower
        //    也必须在整个异步结算窗口期保持第二槽。
        if (high) {
            auto* desired = m_secondSlot ? m_secondSlot->As<RE::TESShout>() : nullptr;
            if (desired && high->currentShout != desired) {
                high->currentShout = desired;
                SKSE::log::warn("AKFS: [defer-restore] currentShout 被引擎换回 {:08X}，重钉回第二槽 {:08X}",
                                high->currentShout ? high->currentShout->GetFormID() : 0,
                                desired->GetFormID());
            }
            if (desired && rt.selectedPower != desired) {
                rt.selectedPower = desired;
                SKSE::log::warn("AKFS: [defer-restore] selectedPower 被引擎换回 {:08X}，重钉回第二槽 {:08X}",
                                m_previousPower ? m_previousPower->GetFormID() : 0,
                                desired->GetFormID());
            }
        }

        // 3) 条件还原：voiceState 归零（kNone=施放彻底结束）→ 还原；
        //    否则继续挂着让异步结算吃到第二槽，直到 m_restoreAt（3s 硬上限）兜底。
        const bool voiceIdle = high && high->voiceState.underlying() == 0;
        const bool hardTimeout = now >= m_restoreAt;
        if (!voiceIdle && !hardTimeout)
            return;

        SKSE::log::info("AKFS: [defer-restore] {}，还原 currentShout+selectedPower",
                        voiceIdle ? "voiceState 已归零" : "到达 3s 硬上限");
        RestoreCurrentShout();   // 内部会清 pending
        // 还原后只读快照：确认 currentShout 归位后，voice 状态机/变体残留是否仍驱动红字。
        if (player && player->GetActorRuntimeData().currentProcess
            && player->GetActorRuntimeData().currentProcess->high) {
            RE::HighProcessData* h = player->GetActorRuntimeData().currentProcess->high;
            auto* nowShout = player->GetCurrentShout();
            SKSE::log::info("AKFS: [defer-restore] 还原后快照 currentShout={:08X} voiceState={} currentShoutVariation={} selectedPower={:08X}",
                            nowShout ? nowShout->GetFormID() : 0,
                            static_cast<std::uint32_t>(h->voiceState.underlying()),
                            static_cast<std::uint32_t>(h->currentShoutVariation),
                            player->GetActorRuntimeData().selectedPower
                                ? player->GetActorRuntimeData().selectedPower->GetFormID() : 0);
        }
    }

    // 立即施放力量/能力（CastSpellImmediate 是唯一已证可靠原语）+ 独立能力冷却。
    // ⚠️ v0.19 问题 2 防护：
    //   - 施放前先消化/中止任何残留的龙吼延迟还原（restorePending）——绝不允许 deferred restore
    //     在跨界（能力施放 / 变身状态切换）时残留 currentShout/selectedPower 操纵，那会与游戏的
    //     transform 清除 + 铠甲重挂载交错而崩溃（见 2026-10-13 崩栈：armor/BipedAnim attach 路径）。
    //   - 能力施放本身不碰 currentShout/selectedPower/voiceState，天然干净；故吸血鬼领主下
    //     X→恢复形态(Revert Form, kPower)可安全施放。
    void SecondSlotManager::CastPowerNow()
    {
        auto* spell = m_secondSlot ? m_secondSlot->As<RE::SpellItem>() : nullptr;
        if (!spell) {
            NotifyLocalized("AKFS: 第二槽无法解析为法术", "AKFS: Second slot cannot be resolved to a spell");
            return;
        }
        // 防残留：丢弃可能悬挂的龙吼延迟还原状态（能力跨界不应继承龙吼的临时槽操纵）。
        if (m_restorePending) {
            RestoreCurrentShout();   // 立即归位 currentShout/selectedPower，清 pending
            SKSE::log::info("AKFS: [power] 施放能力前清掉残留的龙吼延迟还原，已归位 currentShout/selectedPower");
        }
        auto* player = Player();
        if (!player)
            return;
        // ⚠️ [DIAG] 只读探针：打印该能力的 SpellType + IsInCastPowerList——看引擎是否标记它"已用过"。
        // 一次性回答"原版特殊能力到底有没有冷却标记"。SpellType: kPower=2 / kLesserPower=3 / kVoicePower=11。
        SKSE::log::info("AKFS: [power-probe] spell={} SpellType={} IsInCastPowerList={}",
                        spell->GetName(),
                        static_cast<std::uint32_t>(spell->GetSpellType()),
                        player->IsInCastPowerList(spell) ? 1 : 0);
        // ⚠️ v2.0 尝试 AddCastPower(原版Z键通道) 施放能力 → 用户实测【无法释放】，与 v0.7 龙吼
        // 在 AE(1.6.1170) 表现一致：AddCastPower 在 AE 不可靠。回退回 **CastSpellImmediate**
        //（引擎最终原语，v0.3/v0.19 实测可靠）。"一天一次"改用只读 CheckCast 门禁另行实现。
        auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) {
            SKSE::log::error("AKFS: 无法取得 kInstant MagicCaster，无法施放");
            return;
        }
        caster->CastSpellImmediate(spell, false, nullptr, 1.0F, false, -1.0F, player);
        // ⚠️ v1.3 有真正的施放才落库"一天一次"（探针实锤本施放原语不写引擎 castPowerList，
        // 故自行记当日施放）。同游戏日内二次按 X 将被 OnCooldown 的 PowerUsedToday 拦截。
        // 跨日(游戏日取整变化)时清掉陈旧条目再记，避免旧日残留误封。
        if (m_secondSlot) {
            const std::uint32_t fid = m_secondSlot->GetFormID();
            // 当天一次仅对"应套当日限制"的能力落库（kPower 且非 DailyBypass）；可反复用能力/后门能力不记录。
            if (AKFSConfig::PowerOncePerDay() && IsDailyPower(spell)) {
                auto* cal = RE::Calendar::GetSingleton();
                float const day = cal ? cal->GetDaysPassed() : 0.0F;
                const auto it = m_powerUsedDay.find(fid);
                if (it == m_powerUsedDay.end() || it->second != day) {
                    // 新的一天(或该书目过期)：清掉旧记录并重记当前日，保证"每游戏日一次"。
                    m_powerUsedDay[fid] = day;
                    SKSE::log::info("AKFS: [daily] 能力 已施放，游戏日={:.0f}", day);
                }
            }
            // ⚠️ v1.1 能力冷却走独立计时（按能力 formID），不再回写 voiceRecoveryTime
            //（那会污染 voice 冷却、反向阻挡下一次龙吼施放）。能力间也不共享冷却——
            // 各能力各自记 m_powerCdUntil[formID]，施放 A 冷却中不影响施放 B。
            m_powerCdUntil[fid] = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(static_cast<std::int64_t>(kPowerRecoveryTime * 1000.0F));
        }
        SKSE::log::info("AKFS: CastSpellImmediate 已施放力量 '{}'（formID={:08X}）独立冷却={:.2f}s",
                        spell->GetName(),
                        m_secondSlot ? m_secondSlot->GetFormID() : 0,
                        kPowerRecoveryTime);
    }

    // 还原临时换下的 currentShout 与 selectedPower（条件延迟还原用；
    // selectedPower 不再在 FinishNativeShout 立即还原——异步施放的声音来源是它，
    // 需一直保持第二槽到 voiceState 归零，见 RestoreCurrentShout 上文说明）。
    void SecondSlotManager::RestoreCurrentShout()
    {
        auto* player = Player();
        if (!player)
            return;
        auto& rt = player->GetActorRuntimeData();
        // ⚠️ v1.2 修复"Z 变 X 槽龙吼"：selectedPower 须**无条件**还原为换槽前的值 m_previousPower
        //（可为 null）。原 `if(m_previousPower)` 在 m_previousPower 为 null（如设槽后第一次施放、
        //  引擎 selectedPower 本为空时）会跳过还原 → selectedPower 残留第二槽龙吼 → 魔法菜单 Z 选中
        //  变成 X 槽龙吼。m_previousPower 在换槽时记录、此处还原、用后置 null，更换严格配对，写 null
        //  不误清玩家选择（v0.15 担心的"写 null 清玩家 Z 选"是 m_previousPower 被提前清零的另一场景，
        //  与本条件无 guarantee 冲突——此处还原的就是玩家原来的值）。日志辅助定位还原前后值。
        SKSE::log::info("AKFS: [restore] selectedPower 还原前 当前={:08X} m_previousPower={:08X}",
                        rt.selectedPower ? rt.selectedPower->GetFormID() : 0,
                        m_previousPower ? m_previousPower->GetFormID() : 0);
        rt.selectedPower = m_previousPower;   // 无条件还原（可为 null）
        m_previousPower = nullptr;
        // currentShout：currentShout 与 selectedPower 分开暂存，各自独立还原。
        if (rt.currentProcess && rt.currentProcess->high)
            rt.currentProcess->high->currentShout = m_previousShout;
        m_previousShout = nullptr;
        m_restorePending = false;
    }
}