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
set(SPDLOG_STAGE "${CMAKE_BINARY_DIR}/deps/spdlog")
if(EXISTS "${SPDLOG_STAGE}/lib/cmake/spdlog/spdlogConfig.cmake")
	list(PREPEND CMAKE_PREFIX_PATH "${SPDLOG_STAGE}")
	message(STATUS "spdlog staged at ${SPDLOG_STAGE}")
else()
	message(FATAL_ERROR
		"spdlog not staged yet. Run scripts/build_spdlog.ps1 first (installs into ${SPDLOG_STAGE}).")
endif()

# 2) CommonLibSSE-NG: 游戏内部 API 库 (支持 SE/AE/VR 多目标动态 runtime)
set(FETCHCONTENT_SOURCE_DIR_COMMONLIBSSE "${CMAKE_SOURCE_DIR}/thirdparty/CommonLibSSE-NG")
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
set(RAPIDCSV_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/thirdparty/rapidcsv/src" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(CommonLibSSE)

# ---- 插件目标 ----
add_library("${PLUGIN_NAME}" MODULE
	src/Version.cpp
	src/main.cpp
	src/AKFSConfig.cpp
	src/SecondSlotManager.cpp
	src/MenuMouseIntercept.cpp
)

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
