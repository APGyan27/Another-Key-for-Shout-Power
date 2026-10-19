// AKFSConfig.cpp — AKFSConfig 实现。
//   用 Win32 GetPrivateProfileString/Int 读插件 DLL 同级的 AKFS.ini（无第三方依赖、Runtime 无关）。
//   ini 键：[Settings] KeyCode=<DIK 扫描码，默认 0x2D=X>；[Lang] Language=<English|中文>。
//   插件自身目录：GetModuleHandleW(L"AKFS.dll") → GetModuleFileNameW 取全路径 → parent_directory。
//
//   v1.7：新增 setter（内存+ini 双写）、[Strings] 本地化查询、Reload()。
#include "AKFSConfig.h"

#include "REX/W32/DINPUT.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace SEMod
{
    bool         AKFSConfig::s_loaded = false;
    std::uint8_t AKFSConfig::s_keyCode = static_cast<std::uint8_t>(REX::W32::DIK_X);
    std::string  AKFSConfig::s_lang = "cn";
    std::string  AKFSConfig::s_iniPath;
    bool         AKFSConfig::s_powerOncePerDay = true;
    std::unordered_set<std::uint32_t> AKFSConfig::s_dailyBypass;
    std::unordered_set<std::uint32_t> AKFSConfig::s_blockedPowers;
    std::unordered_map<std::string, std::string> AKFSConfig::s_strings;

    namespace
    {
        // 插件 DLL 所在目录（即 Data\SKSE\Plugins\）。
        std::filesystem::path PluginDirectory()
        {
            const HMODULE handle = GetModuleHandleW(L"AKFS.dll");
            wchar_t buf[MAX_PATH * 2]{};
            if (handle) {
                const DWORD len = GetModuleFileNameW(handle, buf, static_cast<DWORD>(std::size(buf)));
                if (len > 0 && len < std::size(buf)) {
                    std::filesystem::path p(buf);
                    return p.parent_path();
                }
            }
            // 兜底：相对当前工作目录（一般即游戏根目录下的 Data\SKSE\Plugins）。
            return std::filesystem::path(L"Data\\SKSE\\Plugins");
        }

        // 解析 [Settings] 下一个 formID 清单键（逗号/空白分隔的 8 位 hex）。
        std::unordered_set<std::uint32_t> ParseFormIdList(const char* a_section, const char* a_key, const std::string& a_iniPath)
        {
            std::unordered_set<std::uint32_t> result;
            char list[4096]{};
            if (GetPrivateProfileStringA(a_section, a_key, "", list, sizeof(list), a_iniPath.c_str()) > 0) {
                // 用 strtok 切分（逗号/空白都是分隔符），每段按 16 进制解析。
                char* tok = std::strtok(list, " ,\t\r\n");
                while (tok) {
                    if (std::strlen(tok) > 0) {
                        char* end = nullptr;
                        const auto v = std::strtoul(tok, &end, 16);
                        if (end && *end == '\0' && v != 0)
                            result.insert(static_cast<std::uint32_t>(v));
                    }
                    tok = std::strtok(nullptr, " ,\t\r\n");
                }
            }
            return result;
        }

        // ⚠️ Win32 没有"列出本 ini 节所有键"的 API，故用"已知键清单"逐个查询。
        //    新增本地化文本时：在此数组补一个 key，并在两个语言 ini 的 [Strings] 节各补一行即可。
        const char* const kStringKeys[] = {
            // 分区与页面标题
            "SectionName",
            "PageSettings",
            "PageStatus",
            // 设置页
            "LabelTriggerKey",
            "LabelTriggerKeyHint",
            "LabelPowerOncePerDay",
            "LabelPowerOncePerDayHint",
            "LabelDailyBypass",
            "LabelDailyBypassHint",
            "LabelBlockedPowers",
            "LabelBlockedPowersHint",
            "LabelLanguage",
            "LabelLanguageHint",
            "BtnAdd",
            "BtnRemove",
            "BtnReloadIni",
            "TxtEmptyList",
            "TxtNoSelection",
            // 状态页
            "LabelSecondSlot",
            "LabelZSlot",
            "LabelSlotType",
            "LabelShoutCooldown",
            "LabelPowerCooldown",
            "LabelVoiceState",
            "LabelIniPath",
            "BtnClearSlot",
            "BtnTestCast",
            "TxtNotSet",
            "TxtReady",
            "TxtShoutType",
            "TxtPowerType",
            "TxtInvalidFormId",
        };

    }

    // 重建 [Strings] 缓存（成员函数，因需直接写 s_strings）。
    void AKFSConfig::ReloadStringsFrom(const std::string& a_iniPath)
    {
        s_strings.clear();
        for (const char* key : kStringKeys) {
            char buf[1024]{};
            const DWORD got = GetPrivateProfileStringA("Strings", key, "", buf, sizeof(buf), a_iniPath.c_str());
            if (got > 0)
                s_strings.emplace(key, std::string(buf, got));
        }
    }

    const std::string& AKFSConfig::Load()
    {
        if (s_loaded)
            return s_iniPath;

        s_iniPath = (PluginDirectory() / "AKFS.ini").string();

        char buf[64]{};
        // KeyCode：DirectInput 扫描码，默认 X。
        const int key = GetPrivateProfileIntA("Settings", "KeyCode", REX::W32::DIK_X, s_iniPath.c_str());
        if (key > 0 && key <= 0xFF)
            s_keyCode = static_cast<std::uint8_t>(key);

        // Language：English / 中文（忽略大小写与空白）。
        const DWORD got = GetPrivateProfileStringA("Lang", "Language", "中文", buf, sizeof(buf), s_iniPath.c_str());
        if (got > 0) {
            std::string lang(buf, got);
            for (auto& c : lang)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            // 去掉首尾空白
            while (!lang.empty() && (lang.front() == ' ' || lang.front() == '\t'))
                lang.erase(lang.begin());
            while (!lang.empty() && (lang.back() == ' ' || lang.back() == '\t'))
                lang.pop_back();
            s_lang = lang;
        }

        // v1.3：特殊能力是否"一天一次"（默认开，对齐原版每日能力）。0 关闭 → 只走 3s 独立冷却。
        s_powerOncePerDay = (GetPrivateProfileIntA("Settings", "PowerOncePerDay", 1, s_iniPath.c_str()) != 0);

        // v1.4：后门清单 —— 命中者豁免"当日一次"。
        s_dailyBypass = ParseFormIdList("Settings", "DailyBypass", s_iniPath);

        // v1.6 变身类黑名单（额外 formID 排除）。
        s_blockedPowers = ParseFormIdList("Settings", "BlockedPowers", s_iniPath);

        // v1.7：[Strings] 本地化文本。
        ReloadStringsFrom(s_iniPath);

        s_loaded = true;
        return s_iniPath;
    }

    void AKFSConfig::Reload()
    {
        s_loaded = false;
        Load();
    }

    // v1.6 变身类 power 黑名单。解析 [Settings] BlockedPowers=<8位hex,逗号/空格分隔>。
    // ⚠️ 需求：吸血鬼变身(Vampire Lord)、狼人变身(Beast Form)这类 transform 型 power 走 X 通道
    //    施放会破坏引擎 power 状态机 → 变身后解除崩溃(armor/BipedAnim attach 交错，见 MEMORY v0.19)。
    //    命中此清单的 power 永不存入第二槽、也不施放。默认由 IsTransformationForm 按 archetype
    //    自动拦截，此清单用于用户按 formID 额外排除。
    void AKFSConfig::LoadBlockedPowers()
    {
        if (!s_loaded) {
            Load();
            return;
        }
        s_blockedPowers = ParseFormIdList("Settings", "BlockedPowers", s_iniPath);
    }

    std::uint8_t AKFSConfig::KeyCode()
    {
        if (!s_loaded)
            Load();
        return s_keyCode;
    }

    bool AKFSConfig::English()
    {
        if (!s_loaded)
            Load();
        return s_lang == "english";
    }

    bool AKFSConfig::PowerOncePerDay()
    {
        if (!s_loaded)
            Load();
        return s_powerOncePerDay;
    }

    bool AKFSConfig::DailyBypassContains(const std::uint32_t a_formId)
    {
        if (!s_loaded)
            Load();
        return s_dailyBypass.find(a_formId) != s_dailyBypass.end();
    }

    // v1.6 变身类 power 黑名单判定（formID 精确匹配）。
    bool AKFSConfig::BlockedPowerContains(const std::uint32_t a_formId)
    {
        if (!s_loaded)
            Load();
        return s_blockedPowers.find(a_formId) != s_blockedPowers.end();
    }

    // v1.1 问题 3 修复：把第二槽所选 FormID 持久化进 AKFS.ini（8 位 hex）。formID=0 清空。
    void AKFSConfig::SetSecondSlot(const std::uint32_t a_formId)
    {
        if (!s_loaded)
            Load();
        if (s_iniPath.empty())
            return;
        if (a_formId == 0) {
            WritePrivateProfileStringA("Settings", "SecondSlot", nullptr, s_iniPath.c_str());
            return;
        }
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%08X", a_formId);
        WritePrivateProfileStringA("Settings", "SecondSlot", buf, s_iniPath.c_str());
    }

    // 读取持久化的第二槽 FormID（0 表示未配置）。
    std::uint32_t AKFSConfig::SecondSlotFormID()
    {
        if (!s_loaded)
            Load();
        char buf[32]{};
        const DWORD got = GetPrivateProfileStringA("Settings", "SecondSlot", "", buf, sizeof(buf), s_iniPath.c_str());
        if (got == 0)
            return 0;
        // 兼容 8 位 hex（大写）与纯数字两种形式。
        std::string_view sv(buf, got);
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
            sv.remove_prefix(1);
        while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t'))
            sv.remove_suffix(1);
        if (sv.empty())
            return 0;
        char* end = nullptr;
        const auto v = std::strtoul(sv.data(), &end, 16);
        if (end && *end == '\0')
            return static_cast<std::uint32_t>(v);
        return 0;
    }

    // ==================== v1.7 setter（内存 + ini 双写） ====================

    bool AKFSConfig::SetKeyCode(const std::uint8_t a_keyCode)
    {
        if (!s_loaded)
            Load();
        if (a_keyCode == 0)
            return false;
        s_keyCode = a_keyCode;  // ← 立即生效：SecondSlotManager 下次按键即按新码判定

        if (s_iniPath.empty())
            return false;
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(a_keyCode));
        return WritePrivateProfileStringA("Settings", "KeyCode", buf, s_iniPath.c_str()) != FALSE;
    }

    bool AKFSConfig::SetPowerOncePerDay(const bool a_enabled)
    {
        if (!s_loaded)
            Load();
        s_powerOncePerDay = a_enabled;  // ← 立即生效

        if (s_iniPath.empty())
            return false;
        return WritePrivateProfileStringA("Settings", "PowerOncePerDay", a_enabled ? "1" : "0", s_iniPath.c_str()) != FALSE;
    }

    bool AKFSConfig::SetLanguage(const std::string& a_language)
    {
        if (!s_loaded)
            Load();

        // 规整：trim + 小写比较。English/en → english；中文/cn/anything else → cn。
        std::string normalized = a_language;
        while (!normalized.empty() && (normalized.front() == ' ' || normalized.front() == '\t'))
            normalized.erase(normalized.begin());
        while (!normalized.empty() && (normalized.back() == ' ' || normalized.back() == '\t'))
            normalized.pop_back();
        for (auto& c : normalized)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        const bool wantEnglish = (normalized == "english" || normalized == "en");
        s_lang = wantEnglish ? "english" : "cn";  // ← 立即生效

        if (s_iniPath.empty())
            return false;
        const char* written = wantEnglish ? "English" : "中文";
        const bool ok = WritePrivateProfileStringA("Lang", "Language", written, s_iniPath.c_str()) != FALSE;

        // ⚠️ 语言决定"提示文案读哪一份"（FOMOD 已把对应语言整份拷成 AKFS.ini），
        //    故切换语言后必须重读 [Strings]，菜单文本随之刷新。
        ReloadStringsFrom(s_iniPath);
        return ok;
    }

    std::unordered_set<std::uint32_t> AKFSConfig::DailyBypassSet()
    {
        if (!s_loaded)
            Load();
        return s_dailyBypass;
    }

    std::unordered_set<std::uint32_t> AKFSConfig::BlockedPowersSet()
    {
        if (!s_loaded)
            Load();
        return s_blockedPowers;
    }

    // 把 formID 清单序列化成 "00092C48,000D2051" 并写入 ini。
    bool AKFSConfig::WriteFormIdList(const char* a_key, const std::unordered_set<std::uint32_t>& a_set)
    {
        if (s_iniPath.empty())
            return false;
        if (a_set.empty()) {
            // 写空串而非删键：保持 ini 里有可见字段，方便用户手改。
            return WritePrivateProfileStringA("Settings", a_key, "", s_iniPath.c_str()) != FALSE;
        }
        // 排序保证输出稳定（unordered_set 迭代顺序不固定，直接写会让 ini 内容乱跳）。
        std::vector<std::uint32_t> sorted(a_set.begin(), a_set.end());
        std::sort(sorted.begin(), sorted.end());

        std::string joined;
        joined.reserve(sorted.size() * 9);
        for (std::size_t i = 0; i < sorted.size(); ++i) {
            char buf[16]{};
            std::snprintf(buf, sizeof(buf), "%08X", sorted[i]);
            if (i > 0)
                joined.push_back(',');
            joined += buf;
        }
        return WritePrivateProfileStringA("Settings", a_key, joined.c_str(), s_iniPath.c_str()) != FALSE;
    }

    bool AKFSConfig::SetDailyBypass(const std::unordered_set<std::uint32_t>& a_set)
    {
        if (!s_loaded)
            Load();
        s_dailyBypass = a_set;  // ← 立即生效
        return WriteFormIdList("DailyBypass", a_set);
    }

    bool AKFSConfig::SetBlockedPowers(const std::unordered_set<std::uint32_t>& a_set)
    {
        if (!s_loaded)
            Load();
        s_blockedPowers = a_set;  // ← 立即生效
        return WriteFormIdList("BlockedPowers", a_set);
    }

    // ==================== v1.7 本地化 ====================

    const char* AKFSConfig::Text(const char* a_key)
    {
        if (!s_loaded)
            Load();
        if (!a_key)
            return "";
        const auto it = s_strings.find(a_key);
        if (it != s_strings.end())
            return it->second.c_str();
        // 找不到时回退显示 key 本身 —— 一眼能看出漏翻了哪一条。
        return a_key;
    }

    const std::string& AKFSConfig::IniPath()
    {
        if (!s_loaded)
            Load();
        return s_iniPath;
    }
}
