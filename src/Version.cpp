// SKSE 插件版本导出。
// CommonLibSSE-NG 插件必须在某翻译单元调用 SKSEPluginInfo 宏，否则 SKSE 报
// "no version data (handle 0)"，从不调用 SKSEPlugin_Load，插件等于没加载。
// 发布版本号采用三位结构 major.minor.patch（此处 1.0.2）；内部迭代号见 CHANGELOG（v1.5）。
#include "SKSE/Impl/PCH.h"
#include "SKSE/API.h"

SKSEPluginInfo({
    .Version = { 1, 0, 2, 0 },
    .Name = "Another Key for Shout",
    .Author = "Another Key for Shout",
    .SupportEmail = "",
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
});