// AKFSConfig.h — 插件运行期配置（读取 Data\SKSE\Plugins\AKFS.ini，与 DLL 同级）。
//   让用户在不动源码/不重编译的前提下改配置：
//   ① 触发键位（KeyCode，DirectInput 扫描码，默认 0x2D = X）；
//   ② 提示文案语言（Language = English / 中文）；
//   ③ 特殊能力"一天一次"开关与豁免/黑名单清单。
//
//   v1.7 新增：SKSE Menu Framework 菜单前端。
//   ⚠️ 架构约定（C 方案）：**内存为权威 + ini 持久化**。
//      - 业务代码（SecondSlotManager 等）全程读内存静态变量，故改配置必须写内存才"立即生效"；
//      - ini 仅用于"下次启动恢复"，故 setter 在写内存的同时一并落盘，双写一次完成。
//      - 因为 Load() 有 s_loaded 早退保护、不会重读 ini，只改 ini 对当前会话毫无作用。
#pragma once

#include "SKSE/Impl/PCH.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace SEMod
{
    class AKFSConfig
    {
    public:
        // 读取一次并缓存。返回 DLL 所在目录下 AKFS.ini 的绝对路径（用于日志）。
        static const std::string& Load();

        // 强制丢弃缓存并重读 ini（供菜单"重新加载"按钮使用）。
        static void Reload();

        // 配置访问器（Load 后调用）。
        static std::uint8_t KeyCode();          // 触发键 DIK 码，默认 0x2D(45=X)
        static bool         English();          // true=英文提示, false=中文

        // ⚠️ v1.1 持久化第二槽（问题 3 修复）：把 X 槽记录的龙吼/能力的 FormID 写入 AKFS.ini，
        //   退出游戏重进后仍保留所选第二槽。formID=0 表示清空。
        static void         SetSecondSlot(std::uint32_t a_formId);
        // 读取持久化的第二槽 FormID；未配置/不存在返回 0。
        static std::uint32_t SecondSlotFormID();

        // v1.3 一天一次（特殊能力）。原版"每天一次"靠引擎 castPowerList 标记，但我们的施放原语
        // CastSpellImmediate 不写它（探针实测 IsInCastPowerList 恒为 0），故自行用游戏日跟踪。
        // ini [Settings] PowerOncePerDay=1 开 / 0 关（默认 1，对齐原版每日能力如"高贵血统"）。
        static bool          PowerOncePerDay();
        // v1.4 后门：ini [Settings] DailyBypass=<formID1>,<formID2>,…（8 位 hex，逗号/空格分隔）。
        //   命中该清单的 kPower 能力**不套"当日一次"门禁**（应对"类型是 kPower 但你可反复用"的特殊组合，
        //   如 Beast Form + 净化过的 Ring of Hircine 可反复变身）。
        static bool          DailyBypassContains(std::uint32_t a_formId);

        // v1.6 变身类 power 黑名单：ini [Settings] BlockedPowers=<formID1>,<formID2>,…（8 位 hex，
        //   逗号/空格分隔）。命中者 **永不** 存入 X 第二槽、也从不施放。
        //   默认自动排除变身 archetype(吸血鬼领主/狼人)，此清单用于用户额外排除其它不想走 X 的 power
        //   （如"每日一次"但不想误触发的变身类）。返回值 true=该 formID 被列入黑名单。
        static bool          BlockedPowerContains(std::uint32_t a_formId);

        // ==================== v1.7 菜单前端所需的写入接口 ====================
        // 所有 Set* 均为"双写"：先改内存（立即生效），再写 AKFS.ini（持久化）。
        // 返回 true 表示内存已更新且 ini 写入成功；ini 写入失败不影响内存生效。

        // 触发键位（DIK 扫描码）。
        static bool SetKeyCode(std::uint8_t a_keyCode);
        // 特殊能力"一天一次"开关。
        static bool SetPowerOncePerDay(bool a_enabled);
        // 语言：[Lang] Language。a_language 传 "English" 或 "中文"（也接受 english/cn）。
        //   切换后立即按新语言重读 [Strings] 节，菜单文本随之刷新。
        static bool SetLanguage(const std::string& a_language);

        // DailyBypass / BlockedPowers 清单的读写。
        static std::unordered_set<std::uint32_t> DailyBypassSet();
        static std::unordered_set<std::uint32_t> BlockedPowersSet();
        // 整体替换清单（菜单里增删后调用）。内部会序列化为逗号分隔的 8 位 hex 写入 ini。
        static bool SetDailyBypass(const std::unordered_set<std::uint32_t>& a_set);
        static bool SetBlockedPowers(const std::unordered_set<std::uint32_t>& a_set);

        // ==================== v1.7 本地化 ====================
        // 读取 ini [Strings] 节中的键值；找不到时返回 key 本身（便于定位漏翻项）。
        // 返回值生命周期与内部缓存一致（直到下次 Reload/Load）。
        static const char* Text(const char* a_key);

        // 当前 ini 绝对路径（菜单状态页展示用）。
        static const std::string& IniPath();

        // 重建 [Strings] 缓存（Load / SetLanguage / Reload 内部使用）。
        static void ReloadStringsFrom(const std::string& a_iniPath);

    private:
        static bool         s_loaded;           // 是否已读取
        static std::uint8_t s_keyCode;          // 触发键扫描码
        static std::string  s_lang;             // 语言字符串(小写后比较)
        static std::string  s_iniPath;          // ini 绝对路径（日志用）
        static bool         s_powerOncePerDay;  // 特殊能力是否"一天一次"
        static std::unordered_set<std::uint32_t> s_dailyBypass;  // 豁免"当日一次"的 formID 集合
        static std::unordered_set<std::uint32_t> s_blockedPowers;  // v1.6 禁止存入/施放的 formID 集合

        // [Strings] 节缓存（key → 本地化文本）。切换语言时整体重建。
        static std::unordered_map<std::string, std::string> s_strings;

        // v1.6 解析 [Settings] BlockedPowers 黑名单表单。Load() 内部调用一次（需访问类静态成员）。
        static void LoadBlockedPowers();

        // 把 formID 集合序列化为 "AABBCCDD,11223344" 形式并写入 ini 的指定键。
        static bool WriteFormIdList(const char* a_key, const std::unordered_set<std::uint32_t>& a_set);

        // 已知的 [Strings] 键清单（Win32 无法枚举节内键，只能逐个查询）。
        static const char* const* StringKeys(std::size_t& a_count);
    };
}
