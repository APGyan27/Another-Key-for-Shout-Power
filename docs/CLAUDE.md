# CLAUDE.md — 写码规定与习惯

> 本文件(CLAUDE.md)是**编码规约与工作习惯的权威文件**：语言风格、命名、结构、构建、提交习惯、审查清单。任何 AI 助手/协作者在写代码前必须先读它。
>
> 职责边界：
> - **项目记忆/决定** → 见 `MEMORY.md`
> - **结构/实体清单** → 见 `DATA.md`

---

## 1. 总体原则

1. **以 CommonLibSSE-NG 为准**：凡涉及游戏内部 API/类型的操作，一律走 CommonLibSSE-NG 提供的接口，**永不手写游戏内部地址**。地址兼容交给 Address Library。
2. **Runtime 无关**：代码要能在 1.6.x(尤其 1.6.1170)通用运行，不绑定单一版本专属偏移。
3. **可读性优先**：代码是给人读的。清晰 > 炫技。
4. **小步前进**：先跑通最小骨架，再加功能。

---

## 2. 语言与注释

- 代码标识符(变量/函数/类/文件名)**全部使用英文**。
- 注释、文档、提交信息：**中文为主**(本项目由中文用户主导)，关键 API 说明可中英夹杂。
- 注释要说明「为什么」，少写「是什么」(代码自己说明是什么)。
- 重要函数/类加中文 docblock 说明用途与副作用。
- 对外(未来可能开源)面向用户的部分建议双语。

---

## 3. 命名与风格规约

- 语言：**C++23**（CommonLibSSE-NG 3.6.0 要求 C++23，非 C++20；`cxx_std_23`）。
- 命名：
  - 类/结构体：`PascalCase` (如 `MyPluginService`)
  - 成员变量：`m_` 前缀 + `PascalCase` (如 `m_pluginApi`)
  - 函数/方法：`PascalCase` (如 `RegisterListener`)
  - 常量：`k` 前缀 + `PascalCase` (如 `kPluginVersion`) 或 `UPPER_SNAKE`
  - 局部变量/参数：`camelCase` (如 `const auto& currentRef`)
  - 命名空间 / 文件：`snake_case`
  - 一般不用匈牙利前缀(常量 `k`、成员 `m_` 除外)。
- 缩进：**4 空格**，不用 Tab。
- 花括号：K&R 风格(左花括号随语句同行)。
- 指针/引用：`Guid* guid` 与 `const char&` — 符号贴近类型名。
- `const` 正确性：能用则用。拷贝昂贵且不需要修改时用 `const&` 传参。
- RAII 优先；尽量少用裸 `new/delete`。
- 禁止 `using namespace std;`(在头文件里绝对禁止)。

---

## 4. 项目结构与文件规约

- 目录/文件布局见 `DATA.md`。
- 头文件：`#pragma once`，而不是 include guard 宏。
- 尽量**前向声明**，减少 `#include` 重型头文件(如 `obfuscation` 相关)以加快编译。
- 插件入口 `SKSEPlugin_Load` 集中在一个文件，只在入口做依赖注册与消息监听，业务逻辑分离到各 Service 类。

---

## 5. 构建与工具链

- **构建系统**：CMake Min 版本 3.19+，`add_library(MODULE)`，目标是 Windows DLL(本插件的 `SEMod` 用它自身的 `CMakeLists.txt`)。
- **架构**：x64；`Release`/`RelWithDebInfo`；启用 `/bigobj` 与 `/utf-8`。
- **生成器/工具集(本机关键)**：用 **`-G "Visual Studio 17 2022" -A x64 -T v145`**。
  - ⚠️ 本机 Build Tools 平台工具集是 **v145**（VS 2026），默认 v143 会报 `MSB8020`。
  - ⚠️ **不要用 Ninja**：本机 Ninja+MSVC 会卡在 ABI 探测；VS generator 正常。
- **依赖获取(已定稿，不走 vcpkg)**：依赖 git clone 到 `thirdparty/`（`spdlog`、`CommonLibSSE-NG`、`rapidcsv`）。
  - spdlog 需先 build+install 到 `build/deps/spdlog`（脚本 `scripts/build_spdlog.ps1`），因 CommonLibSSE 依赖 `find_package(spdlog CONFIG)`。
  - rapidcsv 头文件位置设 `RAPIDCSV_INCLUDE_DIRS=thirdparty/rapidcsv/src`。
  - 地址解析由 **Address Library** 在运行期提供，不要编译期写死地址。
- **C++ 标准**：`cxx_std_23`（CommonLibSSE-NG 3.6.0 要求 C++23，非 C++20）。
- **⚠️ 路径含空格**：项目在 `E:\SE mod`(带空格)。所有 CMake 路径要用带引号字面量；`cmake --install ... --prefix=<path>` 用等号单参数；避免 `cd` 传空路径。
- **运行环境**：把生成的 DLL 放到游戏 `Data\SKSE\Plugins\` 下；Address Library 需已安装。SKSE 的 `Relocation` 系统用于测试加载。
- **产物输出**：插件 DLL 输出到 `build/out/<config>/SEMod.dll`。⚠️ MODULE 库(.dll)在 MSVC 下归 **`LIBRARY_OUTPUT_DIRECTORY`** 管(非 `RUNTIME_OUTPUT_DIRECTORY`)，两者都要设，否则落回默认 `build/Release/`。
- **编译命令示例**(在 vcvars64 环境/VS 开发者命令行下)：
  ```
  cmake -S E:\SE mod -B E:\SE mod\build -G "Visual Studio 17 2022" -A x64 -T v145
  cmake --build E:\SE mod\build --config Release
  ```
  注：cmake 不在 PATH，普通 pwsh 下用完整路径(见 MEMORY §4)；编译后可 `dumpbin /exports` 验证是否含 `SKSEPlugin_Load`。
- 有新的构建/环境事实，回填到 `MEMORY.md` 的「教训」。

---

## 6. 插件生命周期约定 (SKSE)

- 在 `SKSEPlugin_Load(SKSE::LoadInterface* skse)` 里：
  - 通过 `SKSE::Init(skse)` 初始化。
  - `GetMessagingInterface()->RegisterListener(...)` 注册消息回调。
  - 处理时机：`kPostLoad`(注册 hook)、`kDataLoaded`(接触已加载数据)。
  - **不要**在 Load 阶段直接读写尚未就绪的游戏数据。
- 包版本信息用 `SKSE::Version`/导出表描述，方便 SKSE 校验。

---

## 7. 错误处理与日志

- 日志优先用 CommonLibSSE-NG 的 `logger::info` / `logger::error`，输出到 `My Games\Skyrim Special Edition\SKSE\*.log`。
- 关键路径必须日志化，便于排查「插件没加载」类问题。
- 返回错误/异常时给出有意义的中文信息(条件允许)。

---

## 8. 提交与工作习惯 (Git)

- 小粒度、可编译的原子提交；每个提交只做一件事。
- 提交信息按 Conventional Commits(可用中文 description)：
  - `feat: 新增 XXX 功能`
  - `fix: 修复 XXX`
  - `refactor: 重构 XXX`
  - `chore: 构建/依赖调整`
  - `docs: 文档更新`
- 提交前：确认编译通过，检查无调试残留/无用 include。
- 新文件记得纳入正确目录并维护 `DATA.md` 结构清单。

---

## 9. 审查清单 (Do / Don't)

**Do**
- 用 CommonLibSSE-NG API 访问游戏数据。
- 依赖注入+消息监听，分阶段初始化。
- `const`、RAII、前向声明、模块化。

**Don't**
- ❌ 不写死游戏内部绝对地址。
- ❌ 不用 `new` 管理长生命周期资源(用 `unique_ptr`/栈)。
- ❌ 不在 `SKSEPlugin_Load` 里直接操作未就绪的数据。
- ❌ 不提交编译不过、含调试残留的代码。
- ❌ 不在头文件 `using namespace std;`。
