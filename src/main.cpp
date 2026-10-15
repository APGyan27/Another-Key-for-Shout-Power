// main.cpp — SEMod 入口 (v0.3 全新重写, CastSpellImmediate 通道)
#include "SKSE/Impl/PCH.h"
#include "SKSE/API.h"
#include "SKSE/Logger.h"

#include "SecondSlotManager.h"
#include "MenuMouseIntercept.h"
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
    // 自身从不建 logger/file sink。不初始化则所有日志都是空操作、不生成 SEMod.log。
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
    if (const auto logDir = SKSE::log::log_directory()) {
        const auto logPath = *logDir / (std::string(SKSE::PluginDeclaration::GetSingleton()->GetName()) + ".log");
        const auto logger = spdlog::basic_logger_mt("default", logPath.string(), true);
        spdlog::set_default_logger(logger);
    } else {
        SKSE::stl::report_and_error("AKFS: 无法取得日志目录");
    }

    SKSE::log::info("AKFS (Another Key for Shout) v0.3.0.0 loaded (CastSpellImmediate path)");

    if (const auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener("SKSE", MessageHandler);
    }

    return true;
}