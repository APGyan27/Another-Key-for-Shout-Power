// AKFSConfig.h — 插件运行期配置（读取 Data\SKSE\Plugins\AKFS.ini，与 DLL 同级）。
//   让用户在不动源码/不重编译的前提下改两件事：
//   ① 触发键位（KeyCode，DirectInput 扫描码，默认 0x2D = X）；
//   ② 提示文案语言（Language = English / 中文）。
//   FOMOD 安装器负责在安装时按用户选择写这个 ini，故 DLL 只需在启动读它一次。
#pragma once

#include "SKSE/Impl/PCH.h"

#include <cstdint>
#include <string>
#include <filesystem>
#include <unordered_set>

namespace SEMod
{
    class AKFSConfig
    {
    public:
        // 读取一次并缓存。返回 DLL 所在目录下 AKFS.ini 的绝对路径（用于日志）。
        static const std::string& Load();

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

    private:
        static bool         s_loaded;           // 是否已读取
        static std::uint8_t s_keyCode;          // 触发键扫描码
        static std::string  s_lang;             // 语言字符串(小写后比较)
        static std::string  s_iniPath;          // ini 绝对路径（日志用）
        static bool         s_powerOncePerDay;  // 特殊能力是否"一天一次"
        static std::unordered_set<std::uint32_t> s_dailyBypass;  // 豁免"当日一次"的 formID 集合
    };
}