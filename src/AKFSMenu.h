// AKFSMenu.h — SKSE Menu Framework 前端（纯 C++，无 Papyrus / 无 ESP / 无 SkyUI）。
//
// 依赖：用户的 Data\SKSE\Plugins\ 下装有 SKSEMenuFramework.dll（前置插件）。
//   未安装时 Register() 直接返回，插件其余功能（X 键第二槽）照常工作 —— 即可选依赖。
//
// 两个页面：
//   [设置] 触发键 / 能力每日一次 / DailyBypass / BlockedPowers / 界面语言
//   [状态] 第二槽、Z 槽、冷却、voiceState、ini 路径 + 清空/重载/测试施放
//
// ⚠️ 头文件刻意不 include 任何 windows.h / SKSEMenuFramework.h：
//    前者会带来 min/max/small 宏污染（曾导致 TESFile.h C2229），后者 520KB 且需要
//    windows.h。二者都只在 .cpp 内引入。
#pragma once

namespace SEMod::AKFSMenu
{
    // 注册菜单页面。应在 SKSEPlugin_Load 里（SKSE::Init 之后）调用，**不要**放在 kDataLoaded：
    // 菜单框架自身在 SKSE 消息早期就绪，早注册可保证主菜单项尽早出现。
    void Register();

    // 是否成功注册（未装 SKSE Menu Framework 时为 false）。
    bool IsRegistered();
}
