// AKFSConfig.cpp — AKFSConfig 实现。
//   用 Win32 GetPrivateProfileString/Int 读插件 DLL 同级的 AKFS.ini（无第三方依赖、Runtime 无关）。
//   ini 键：[Settings] KeyCode=<DIK 扫描码，默认 0x2D=X>；[Lang] Language=<English|中文>。
//   插件自身目录：GetModuleHandleW(L"AKFS.dll") → GetModuleFileNameW 取全路径 → parent_directory。
#include "AKFSConfig.h"

#include "REX/W32/DINPUT.h"

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace SEMod
{
    bool         AKFSConfig::s_loaded = false;
    std::uint8_t AKFSConfig::s_keyCode = static_cast<std::uint8_t>(REX::W32::DIK_X);
    std::string  AKFSConfig::s_lang = "cn";
    std::string  AKFSConfig::s_iniPath;
    bool         AKFSConfig::s_powerOncePerDay = true;
    std::unordered_set<std::uint32_t> AKFSConfig::s_dailyBypass;

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
    }

    const std::string& AKFSConfig::Load()
    {
        if (s_loaded)
            return s_iniPath;

        s_iniPath = (PluginDirectory() / "AKFS.ini").string();

        char buf[64]{};
        // KeyCode：DirectInput 扫描码，默认 X(57)。
        const int key = GetPrivateProfileIntA("Settings", "KeyCode", REX::W32::DIK_X, s_iniPath.c_str());
        if (key > 0 && key <= 0xFF)
            s_keyCode = static_cast<std::uint8_t>(key);

        // Language：English / 中文（忽略大小写与空白）。
        const DWORD got = GetPrivateProfileStringA("Lang", "Language", "中文", buf, sizeof(buf), s_iniPath.c_str());
        if (got > 0) {
            std::string lang(buf);
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

        // v1.4：后门清单 —— 逗号/空格分隔的 8 位 hex formID，命中者豁免"当日一次"。
        s_dailyBypass.clear();
        {
            char list[1024]{};
            if (GetPrivateProfileStringA("Settings", "DailyBypass", "", list, sizeof(list), s_iniPath.c_str()) > 0) {
                // 用 strtok 切分（逗号/空白都是分隔符），每段按 16 进制解析。
                char* tok = std::strtok(list, " ,\t\r\n");
                while (tok) {
                    if (std::strlen(tok) > 0) {
                        char* end = nullptr;
                        const auto v = std::strtoul(tok, &end, 16);
                        if (end && *end == '\0' && v != 0)
                            s_dailyBypass.insert(static_cast<std::uint32_t>(v));
                    }
                    tok = std::strtok(nullptr, " ,\t\r\n");
                }
            }
        }

        s_loaded = true;
        return s_iniPath;
    }

    std::uint8_t AKFSConfig::KeyCode()
    {
        return s_keyCode;
    }

    bool AKFSConfig::English()
    {
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
        std::string_view sv(buf);
        // 去掉首尾空白
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
}