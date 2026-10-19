// AKFSMenu.cpp — SKSE Menu Framework 前端实现。
//
// ⚠️ 编译顺序雷区（本项目已踩过两次，见 docs/MEMORY.md §4）：
//    1. 每个 .cpp 必须先 include "SKSE/Impl/PCH.h"，否则 std::uint32_t/size_t 等环境缺失。
//    2. <windows.h> 绝不能进头文件；在 .cpp 里也必须放在所有 CommonLib 头之后，
//       否则 min/max/small 宏会污染 TESFile.h → C2229 大小为零数组。
//    故本文件顺序：PCH → CommonLib → AKFS 自家头 → windows.h → SKSEMenuFramework.h。
#include "SKSE/Impl/PCH.h"
#include "SKSE/Logger.h"

// ⚠️ 菜单代码要用到 TESForm/TESShout/ButtonEvent 的完整定义（不只前向声明）。
#include "RE/A/AIProcess.h"
#include "RE/B/ButtonEvent.h"
#include "RE/H/HighProcessData.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESShout.h"

#include "AKFSConfig.h"
#include "AKFSMenu.h"
#include "SecondSlotManager.h"

// ---- windows.h 及其后：菜单框架头文件（内含 windows.h）----
#include <windows.h>
#include "SKSEMenuFramework.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace SEMod::AKFSMenu
{
    namespace
    {
        std::atomic<bool> g_registered{ false };

        // 分区名。框架按此名在左侧列表分组。
        constexpr const char* kSectionKey = "AKFS";

        // ---- 菜单内的编辑态（用户操作后立即经 AKFSConfig 双写）----
        // 拖滑块时每帧都会触发返回值 true，若每帧都写 ini 会造成大量磁盘 IO。
        // 故控件编辑的是这两个临时缓冲，**松手/回车时**才真正提交。
        int  g_keyCodeEdit = -1;      // -1 = 未初始化
        bool g_keyCodeDirty = false;  // 需要提交
        bool g_capturingKey = false;  // 是否正在等待用户按键

        // DailyBypass / BlockedPowers 的编辑缓冲（formID 集合 + 输入框）
        std::unordered_set<std::uint32_t> g_bypassEdit;
        std::unordered_set<std::uint32_t> g_blockedEdit;
        char g_bypassInput[16] = {};
        char g_blockedInput[16] = {};
        bool g_buffersLoaded = false;
        // 当前选中的清单条目（用于删除）。用 formID 而非迭代器，避免集合变动后失效。
        std::uint32_t g_selectedBypass = 0;
        std::uint32_t g_selectedBlocked = 0;

        // 语言：0=中文 1=English
        int g_languageIndex = -1;

        // 状态页的一次性动作（按钮按下后由下一次帧消费，避免在渲染中途改游戏状态）。
        bool g_pendingTestCast = false;
        bool g_pendingClearSlot = false;
        bool g_pendingReloadIni = false;

        // 最近一次操作的结果提示（显示在页面顶部，便于确认是否写入成功）。
        std::string g_lastMessage;

        const char* T(const char* a_key)
        {
            return AKFSConfig::Text(a_key);
        }

        // 从配置同步到编辑缓冲（首次打开页面 / 重载 ini / 切换语言后调用）。
        void SyncFromConfig()
        {
            g_keyCodeEdit = static_cast<int>(AKFSConfig::KeyCode());
            g_bypassEdit = AKFSConfig::DailyBypassSet();
            g_blockedEdit = AKFSConfig::BlockedPowersSet();
            g_languageIndex = AKFSConfig::English() ? 1 : 0;
            g_bypassInput[0] = '\0';
            g_blockedInput[0] = '\0';
            g_selectedBypass = 0;
            g_selectedBlocked = 0;
            g_buffersLoaded = true;
        }

        // 把输入框里的 8 位 hex 解析成 formID；解析失败返回 0。
        std::uint32_t ParseFormIdInput(const char* a_text)
        {
            if (!a_text || a_text[0] == '\0')
                return 0;
            // 允许带 "0x" 前缀与首尾空白。
            const char* p = a_text;
            while (*p == ' ' || *p == '\t')
                ++p;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
                p += 2;
            char* end = nullptr;
            const auto v = std::strtoul(p, &end, 16);
            if (end == p)  // 一个字符都没解析出来
                return 0;
            while (end && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
                ++end;
            if (end && *end != '\0')
                return 0;  // 尾部有垃圾字符
            return static_cast<std::uint32_t>(v);
        }

        // 渲染一个 formID 清单编辑器（输入 + 添加 + 列表 + 删除）。
        //   a_set        被编辑的集合（已与配置同步）
        //   a_key        配置键名（提交时用）
        //   a_input      输入缓冲
        //   a_selected   当前选中条目的 formID
        //   a_isBypass   true=DailyBypass false=BlockedPowers
        // ⚠️ 两个清单编辑器在同一页面同时渲染，ImGui 靠标签字符串生成控件 ID。
        //  若两处都用 "##input"/"##list"/同名按钮，ID 会撞车 → 打字时两个框同时收到输入。
        //  故用 PushID(a_key) 把每个编辑器的所有控件 ID 限定在各自命名空间内。
        void RenderFormIdListEditor(std::unordered_set<std::uint32_t>& a_set,
                                    const char* a_key,
                                    char* a_input,
                                    std::uint32_t& a_selected,
                                    bool a_isBypass)
        {
            ImGuiMCP::PushID(a_key);
            // 输入行：文本框 + 添加按钮
            ImGuiMCP::SetNextItemWidth(120.0F);
            // ⚠️ 标签刻意留空（"##"前后无可见文字）：保持原有外观，唯一性由外层 PushID(a_key) 保证。
            ImGuiMCP::InputText("##input", a_input, 15);
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(T("BtnAdd"))) {
                const std::uint32_t fid = ParseFormIdInput(a_input);
                if (fid != 0) {
                    a_set.insert(fid);
                    if (a_isBypass)
                        AKFSConfig::SetDailyBypass(a_set);
                    else
                        AKFSConfig::SetBlockedPowers(a_set);
                    g_lastMessage = std::string("OK: ") + a_key;
                    a_input[0] = '\0';
                } else {
                    g_lastMessage = T("TxtInvalidFormId");
                }
            }

            // 列表：固定高度子窗口，每条可点选，选中后可删。
            //   ⚠️ BeginChild 的 ID 同样受 PushID 作用域保护。
            ImGuiMCP::BeginChild("##list", ImGuiMCP::ImVec2(0.0F, 110.0F), ImGuiMCP::ImGuiChildFlags_Border);
            if (a_set.empty()) {
                ImGuiMCP::TextDisabled(T("TxtEmptyList"));
            } else {
                // 排序后展示，保证顺序稳定。
                std::vector<std::uint32_t> items(a_set.begin(), a_set.end());
                std::sort(items.begin(), items.end());
                for (const auto fid : items) {
                    // ⚠️ 隐式 ID 后缀用 "%08X###" 形式避开可见文本冲突；此处已在外层 PushID 内，
                    //   但同名 formID 在两个清单里仍会各占各的 ID（不同 PushID 作用域）。
                    char label[32]{};
                    std::snprintf(label, sizeof(label), "%08X###fid_%08X", fid, fid);
                    const bool selected = (a_selected == fid);
                    if (ImGuiMCP::Selectable(label, selected))
                        a_selected = fid;
                }
            }
            ImGuiMCP::EndChild();

            // 删除选中项
            if (a_selected != 0 && a_set.find(a_selected) != a_set.end()) {
                if (ImGuiMCP::Button(T("BtnRemove"))) {
                    a_set.erase(a_selected);
                    if (a_isBypass)
                        AKFSConfig::SetDailyBypass(a_set);
                    else
                        AKFSConfig::SetBlockedPowers(a_set);
                    a_selected = 0;
                    g_lastMessage = std::string("OK: ") + a_key;
                }
            } else if (ImGuiMCP::Button(T("BtnRemove"))) {
                g_lastMessage = T("TxtNoSelection");
            }
            ImGuiMCP::PopID();
        }

        // ---------------- 页面 1：设置 ----------------
        void __stdcall RenderSettings()
        {
            if (!g_buffersLoaded)
                SyncFromConfig();

            // ---- 操作结果提示 ----
            if (!g_lastMessage.empty()) {
                ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.85F, 0.45F, 1.0F), "%s", g_lastMessage.c_str());
                ImGuiMCP::Separator();
            }

            // ---- 触发键 ----
            ImGuiMCP::Text("%s", T("LabelTriggerKey"));
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered())
                ImGuiMCP::SetTooltip("%s", T("LabelTriggerKeyHint"));

            ImGuiMCP::SetNextItemWidth(120.0F);
            if (ImGuiMCP::SliderInt("##keycode", &g_keyCodeEdit, 1, 255, "0x%02X")) {
                g_keyCodeDirty = true;  // 拖动中只记脏，松手再提交
            }
            const bool keyReleased = ImGuiMCP::IsItemDeactivatedAfterEdit();
            ImGuiMCP::SameLine();
            // 物理按键捕获：点击后按任意键即取该键扫描码。
            if (ImGuiMCP::Button(g_capturingKey ? "..." : "Capture")) {
                g_capturingKey = !g_capturingKey;
            }
            if (g_keyCodeDirty && keyReleased) {
                AKFSConfig::SetKeyCode(static_cast<std::uint8_t>(g_keyCodeEdit));
                g_keyCodeDirty = false;
                g_lastMessage = "OK: KeyCode";
            }
            // 捕获态由 OnKeyCaptured 设置 g_keyCodeEdit 并置脏。

            ImGuiMCP::Separator();

            // ---- 能力每日一次 ----
            bool oncePerDay = AKFSConfig::PowerOncePerDay();
            if (ImGuiMCP::Checkbox(T("LabelPowerOncePerDay"), &oncePerDay)) {
                AKFSConfig::SetPowerOncePerDay(oncePerDay);
                g_lastMessage = "OK: PowerOncePerDay";
            }
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered())
                ImGuiMCP::SetTooltip("%s", T("LabelPowerOncePerDayHint"));

            ImGuiMCP::Separator();

            // ---- DailyBypass ----
            ImGuiMCP::Text("%s", T("LabelDailyBypass"));
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered())
                ImGuiMCP::SetTooltip("%s", T("LabelDailyBypassHint"));
            RenderFormIdListEditor(g_bypassEdit, "DailyBypass", g_bypassInput, g_selectedBypass, true);

            ImGuiMCP::Separator();

            // ---- BlockedPowers ----
            ImGuiMCP::Text("%s", T("LabelBlockedPowers"));
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered())
                ImGuiMCP::SetTooltip("%s", T("LabelBlockedPowersHint"));
            RenderFormIdListEditor(g_blockedEdit, "BlockedPowers", g_blockedInput, g_selectedBlocked, false);

            ImGuiMCP::Separator();

            // ---- 界面语言（同时决定 AKFS.ini 的提示文案语言）----
            ImGuiMCP::Text("%s", T("LabelLanguage"));
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("(?)");
            if (ImGuiMCP::IsItemHovered())
                ImGuiMCP::SetTooltip("%s", T("LabelLanguageHint"));

            const char* langItems[] = { "中文", "English" };
            ImGuiMCP::SetNextItemWidth(140.0F);
            if (ImGuiMCP::Combo("##lang", &g_languageIndex, langItems, 2)) {
                AKFSConfig::SetLanguage(g_languageIndex == 1 ? "English" : "中文");
                g_lastMessage = "OK: Language";
                // ⚠️ 语言变了 → [Strings] 已重载 → 重新同步缓冲（提示文案已是新语言）。
                SyncFromConfig();
            }

            ImGuiMCP::Separator();

            // ---- 重新加载 ini ----
            if (ImGuiMCP::Button(T("BtnReloadIni"))) {
                AKFSConfig::Reload();
                SyncFromConfig();
                g_lastMessage = "OK: reloaded";
            }
        }

        // ---------------- 页面 2：状态 ----------------
        void __stdcall RenderStatus()
        {
            auto& mgr = SecondSlotManager::GetSingleton();

            // ---- 第二槽 ----
            const RE::TESForm* slot = mgr.GetSecondSlotForm();
            ImGuiMCP::Text("%s", T("LabelSecondSlot"));
            if (slot) {
                const char* name = slot->GetName();
                const char* typeName = mgr.GetSecondSlotIsShout() ? T("TxtShoutType") : T("TxtPowerType");
                ImGuiMCP::SameLine();
                ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.5F, 0.85F, 1.0F, 1.0F), "%08X  %s  (%s)",
                                      slot->GetFormID(), name ? name : "?", typeName);
            } else {
                ImGuiMCP::SameLine();
                ImGuiMCP::TextDisabled("%s", T("TxtNotSet"));
            }

            // ---- Z 槽 ----
            //   ⚠️ Z 键同一时刻只指向"龙吼"或"能力"之一：
            //     选龙吼 → high->currentShout 有值、selectedPower 可能为空；
            //     选能力 → selectedPower 指向该能力、currentShout 为空。
            //   故两者都要查，并以当前实际指向的那个作为主显示。
            const RE::TESShout* zShout = mgr.GetCurrentZShout();
            const RE::TESForm* zPower = mgr.GetCurrentZPower();
            ImGuiMCP::Text("%s", T("LabelZSlot"));
            ImGuiMCP::SameLine();
            if (zShout) {
                const char* name = zShout->GetName();
                ImGuiMCP::Text("%08X  %s  (%s)", zShout->GetFormID(), name ? name : "?", T("TxtShoutType"));
            } else if (zPower) {
                const char* name = zPower->GetName();
                ImGuiMCP::Text("%08X  %s  (%s)", zPower->GetFormID(), name ? name : "?", T("TxtPowerType"));
            } else {
                ImGuiMCP::TextDisabled("%s", T("TxtNotSet"));
            }

            ImGuiMCP::Separator();

            // ---- 冷却 ----
            ImGuiMCP::Text("%s", T("LabelShoutCooldown"));
            ImGuiMCP::SameLine();
            const float shoutCd = mgr.GetShoutCooldownRemaining();
            if (shoutCd > 0.0F)
                ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.7F, 0.3F, 1.0F), "%.2fs", shoutCd);
            else
                ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.9F, 0.45F, 1.0F), "%s", T("TxtReady"));

            ImGuiMCP::Text("%s", T("LabelPowerCooldown"));
            ImGuiMCP::SameLine();
            // 优先显示第二槽能力的冷却；第二槽不是能力时退回显示 Z 槽能力的冷却（若 Z 选的是能力）。
            const RE::TESForm* powerForCd = nullptr;
            if (slot && !mgr.GetSecondSlotIsShout())
                powerForCd = slot;
            else if (zPower)
                powerForCd = zPower;
            if (powerForCd) {
                const float powerCd = mgr.GetPowerCooldownRemaining(powerForCd->GetFormID());
                if (powerCd > 0.0F)
                    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.7F, 0.3F, 1.0F), "%.2fs", powerCd);
                else
                    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.9F, 0.45F, 1.0F), "%s", T("TxtReady"));
            } else {
                ImGuiMCP::TextDisabled("-");
            }

            ImGuiMCP::Separator();

            // ---- 诊断字段 ----
            const int voiceState = mgr.GetVoiceState();
            ImGuiMCP::Text("%s: %d", T("LabelVoiceState"), voiceState);
            ImGuiMCP::Text("charging: %s   restorePending: %s",
                           mgr.IsShoutCharging() ? "true" : "false",
                           mgr.IsRestorePending() ? "true" : "false");

            ImGuiMCP::Separator();

            // ---- ini 路径 ----
            ImGuiMCP::Text("%s", T("LabelIniPath"));
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("%s", AKFSConfig::IniPath().c_str());

            ImGuiMCP::Separator();

            // ---- 动作按钮 ----
            // ⚠️ 状态改动一律"投递 + 下帧执行"，避免在 ImGui 渲染回调里直接驱动游戏施放。
            if (ImGuiMCP::Button(T("BtnClearSlot"))) {
                g_pendingClearSlot = true;
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(T("BtnReloadIni"))) {
                g_pendingReloadIni = true;
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(T("BtnTestCast"))) {
                g_pendingTestCast = true;
            }
        }

        // 物理按键捕获回调（注册到菜单框架的输入事件）。
        // 返回 true 表示"吃掉"这次输入，避免同时触发游戏动作。
        bool __stdcall OnInputCapture(RE::InputEvent* a_event)
        {
            if (!g_capturingKey)
                return false;
            if (!a_event || a_event->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
                return false;
            auto* button = a_event->AsButtonEvent();
            if (!button)
                return false;
            // 只认按下沿；松开不处理。
            if (button->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
                return false;
            if (button->value < 0.5F)
                return false;
            const auto code = button->GetIDCode();
            if (code == 0 || code > 0xFF)
                return false;
            g_keyCodeEdit = static_cast<int>(code);
            AKFSConfig::SetKeyCode(static_cast<std::uint8_t>(code));
            g_keyCodeDirty = false;
            g_capturingKey = false;
            g_lastMessage = "OK: KeyCode";
            return true;  // 消费掉，防止游戏同时响应
        }

        // 每帧回调（HUD 元素），用于消费"状态页投递的动作"。
        void __stdcall OnFrame()
        {
            auto& mgr = SecondSlotManager::GetSingleton();
            if (g_pendingClearSlot) {
                g_pendingClearSlot = false;
                mgr.ClearSlot();
                SKSE::log::info("AKFS: [menu] 已清空第二槽");
            }
            if (g_pendingReloadIni) {
                g_pendingReloadIni = false;
                AKFSConfig::Reload();
                SyncFromConfig();
                SKSE::log::info("AKFS: [menu] 已重新加载 ini");
            }
            if (g_pendingTestCast) {
                g_pendingTestCast = false;
                const bool ok = mgr.TestCastSecondSlot();
                SKSE::log::info("AKFS: [menu] 测试施放结果 = {}", ok ? "已提交" : "被拦截");
            }
        }
    }

    void Register()
    {
#if defined(AKFS_HAS_SKSEMENUFRAMEWORK)
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("AKFS: 未检测到 SKSE Menu Framework，菜单功能跳过（X 键第二槽不受影响）");
            return;
        }

        // 分区 + 两个页面。
        SKSEMenuFramework::SetSection(kSectionKey);
        SKSEMenuFramework::AddSectionItem(T("PageSettings"), RenderSettings);
        SKSEMenuFramework::AddSectionItem(T("PageStatus"), RenderStatus);

        // 输入捕获（改键）+ 每帧动作消费。
        SKSEMenuFramework::AddInputEvent(OnInputCapture);
        SKSEMenuFramework::AddHudElement(OnFrame);

        g_registered.store(true);
        SKSE::log::info("AKFS: SKSE Menu Framework 菜单已注册 (API v{})",
                        SKSEMenuFramework::GetMenuFrameworkAPIVersion());
#else
        SKSE::log::info("AKFS: 编译时未找到 SKSEMenuFramework.h，菜单功能不可用");
#endif
    }

    bool IsRegistered()
    {
        return g_registered.load();
    }
}
