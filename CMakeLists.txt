cmake_minimum_required(VERSION 3.19)

project(
	SEMod
	LANGUAGES CXX
	VERSION 0.1.0
)

# 全局标准化: CommonLibSSE-NG 头文件依赖 C++20/23 标准库特性(span/string_view/source_location)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成文件名(对应最终 DLL 名)。品牌缩写 AKFS = Another Key for Shout。
set(PLUGIN_NAME "AKFS")

# ---- 依赖: 使用本地 thirdparty 源码 (已 clone) ----
# 若 thirdparty 缺失, 改用下方 GIT_REPOSITORY 联网拉取。
include(FetchContent)

# 1) spdlog: CommonLibSSE-NG 依赖 find_package(spdlog CONFIG)。
#    spdlog 通过独立脚本预构建并 install 到 build/deps/spdlog (见 scripts/build_spdlog.ps1)。
#    查找顺序：先看当前 build 目录，再回退到 archive/AKFS/build 里已预构建好的那份。
set(SPDLOG_STAGE "${CMAKE_BINARY_DIR}/deps/spdlog")
if(NOT EXISTS "${SPDLOG_STAGE}/lib/cmake/spdlog/spdlogConfig.cmake")
	set(SPDLOG_STAGE "${CMAKE_SOURCE_DIR}/build/deps/spdlog")
endif()
if(EXISTS "${SPDLOG_STAGE}/lib/cmake/spdlog/spdlogConfig.cmake")
	list(PREPEND CMAKE_PREFIX_PATH "${SPDLOG_STAGE}")
	message(STATUS "spdlog staged at ${SPDLOG_STAGE}")
else()
	message(FATAL_ERROR
		"spdlog not staged yet. Run scripts/build_spdlog.ps1 first (installs into ${CMAKE_BINARY_DIR}/deps/spdlog).")
endif()

# 2) CommonLibSSE-NG: 游戏内部 API 库 (支持 SE/AE/VR 多目标动态 runtime)
# ⚠️ 源码已迁至 archive/AKFS/，依赖仍在工作区根的 thirdparty/，故向上两级定位工作区根。
get_filename_component(AKFS_WORKSPACE_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
if(NOT EXISTS "${AKFS_WORKSPACE_ROOT}/thirdparty/CommonLibSSE-NG")
	set(AKFS_WORKSPACE_ROOT "${CMAKE_SOURCE_DIR}") # 兜底：archive/ 内自带 thirdparty 的历史结构
endif()
set(FETCHCONTENT_SOURCE_DIR_COMMONLIBSSE "${AKFS_WORKSPACE_ROOT}/thirdparty/CommonLibSSE-NG")
FetchContent_Declare(
	CommonLibSSE
	GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
	GIT_TAG main
	GIT_SHALLOW TRUE
)
# 关闭 CommonLib 自带测试 (避免额外依赖 Catch2)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
# 仅保留本次目标相关 runtime，缩短编译 (1.6.1170 属 AE 线；SE 亦常一并开启以便通用)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
# rapidcsv: CommonLib 依赖的头文件库 (find_path RAPIDCSV_INCLUDE_DIRS)
set(RAPIDCSV_INCLUDE_DIRS "${AKFS_WORKSPACE_ROOT}/thirdparty/rapidcsv/src" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(CommonLibSSE)

# 3) SKSE Menu Framework: 纯头文件 + 运行期 GetProcAddress 调用，不链接任何库。
#    源码随本仓库附带的示例工程提供（仅需 include 目录）。
set(SKSE_MENU_FRAMEWORK_INCLUDE
	"${AKFS_WORKSPACE_ROOT}/SKSE-Menu-Framework-3-Example-master/include")
if(NOT EXISTS "${SKSE_MENU_FRAMEWORK_INCLUDE}/SKSEMenuFramework.h")
	set(SKSE_MENU_FRAMEWORK_INCLUDE "" ) # 缺失则跳过菜单功能编译
	message(WARNING "SKSE Menu Framework header not found; AKFSMenu will be built as a no-op.")
endif()

# ---- 插件目标 ----
add_library("${PLUGIN_NAME}" MODULE
	src/Version.cpp
	src/main.cpp
	src/AKFSConfig.cpp
	src/SecondSlotManager.cpp
	src/MenuMouseIntercept.cpp
	src/AKFSMenu.cpp
)

if(SKSE_MENU_FRAMEWORK_INCLUDE)
	target_include_directories("${PLUGIN_NAME}" PRIVATE "${SKSE_MENU_FRAMEWORK_INCLUDE}")
	target_compile_definitions("${PLUGIN_NAME}" PRIVATE AKFS_HAS_SKSEMENUFRAMEWORK=1)
endif()

target_compile_features(
	"${PLUGIN_NAME}"
	PRIVATE
		cxx_std_23 # CommonLibSSE-NG 3.6.0 要求 C++23
)

target_compile_definitions(
	"${PLUGIN_NAME}"
	PRIVATE
		WINVER=0x0601
		_WIN32_WINNT=0x0601
)

target_link_libraries(
	"${PLUGIN_NAME}"
	PRIVATE
		CommonLibSSE::CommonLibSSE
)

# /bigobj 防止符号过多; 视优化需要可再调
if(MSVC)
	target_compile_options(
		"${PLUGIN_NAME}"
		PRIVATE
			/bigobj
			/utf-8
	)
endif()

# ---- 输出到便于部署的目录: build/out/<config>/ 下的 DLL 与 PDB ----
# MODULE 库(.dll)在 MSVC 下归 LIBRARY_OUTPUT_DIRECTORY 管, 非 RUNTIME_OUTPUT_DIRECTORY,
# 故两者都要设置, 否则 DLL 会落到默认的 build/Release/ 而不是 out/。
set_target_properties(
	"${PLUGIN_NAME}"
	PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>"
		PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/$<CONFIG>"
)
