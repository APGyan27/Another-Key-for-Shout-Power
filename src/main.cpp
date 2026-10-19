// main.cpp — AKFS (Another Key for Shout) 插件入口。
#include "SKSE/Impl/PCH.h"
#include "SKSE/API.h"
#include "SKSE/Logger.h"

#include "AKFSConfig.h"
#include "AKFSMenu.h"
#include "MenuMouseIntercept.h"
#include "SecondSlotManager.h"
#include <spdlog/sinks/basic_file_sink.h>

namespace
{
    // kDataLoaded 之后：游戏数据就绪，注册输入 sink。此阶段才能安全接触已加载数据。
    void OnDataLoaded()
    {
        auto& mgr = SEMod::SecondSlotManager::GetSingleton();
        mgr.RegisterInputSink();
        SKSE::log::info("AKFS: kDataLoaded 已完成，输入 sink 已注册");
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                OnDataLoaded();
                break;
            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    // SKSE::Init 返回 void：内部失败会调用 stl::report_and_fail 终止进程，不会返回。
    SKSE::Init(a_skse);

    // ⚠️ 必须手动初始化 spdlog：CommonLibSSE-NG 的 SKSE::log::* 只调 spdlog::log，
    // 自身从不建 logger/file sink。不初始化则所有日志都是空操作、不生成日志文件。
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
    if (const auto logDir = SKSE::log::log_directory()) {
        const auto logPath = *logDir / (std::string(SKSE::PluginDeclaration::GetSingleton()->GetName()) + ".log");
        const auto logger = spdlog::basic_logger_mt("default", logPath.string(), true);
        spdlog::set_default_logger(logger);
    } else {
        SKSE::stl::report_and_error("AKFS: 无法取得日志目录");
    }

    SKSE::log::info("AKFS (Another Key for Shout) TBD-version loaded");

    // v1.7：先读配置（菜单与 [Strings] 本地化文本都依赖它）。
    //   ⚠️ 这里只读 ini + 解析字符串，不接触任何游戏数据，符合"Load 阶段不碰未就绪数据"约定。
    const auto& iniPath = SEMod::AKFSConfig::Load();
    SKSE::log::info("AKFS: 配置文件 = {}", iniPath);

    // v1.7：注册 SKSE Menu Framework 菜单（未安装该前置插件时内部直接返回，不影响主功能）。
    SEMod::AKFSMenu::Register();

    if (const auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener("SKSE", MessageHandler);
    }

    return true;
}
