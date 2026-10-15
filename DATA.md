# DATA.md — 项目结构与实体

> 本文件(DATA.md)是**项目结构与实体的权威清单**：目录布局、关键文件、实体(form/type/实体对象)定义。它是 CLAUDE.md 中「结构请看 DATA.md」的唯一依据。
>
> 职责边界：
> - **项目记忆/决定/进度** → 见 `MEMORY.md`
> - **编码规范/习惯** → 见 `CLAUDE.md`
> - **这里只放「项目长什么样、有哪些实体」的静态清单**。

---

## 1. 项目概览

| 项 | 值 |
|----|----|
| 项目名 | (待定 SKSE 插件) |
| 类型 | Skyrim SE 插件 (DLL) |
| 目标 Runtime | 1.6.1170 (通用, CommonLibSSE-NG) |
| 核心依赖 | CommonLibSSE-NG + Address Library |
| 入口导出 | `SKSEPlugin_Load` |
| 当前代码量 | 最小骨架已落地并编译通过 (`build/out/Release/SEMod.dll`)

> 状态变化请同步 `MEMORY.md`；此处只保留结构快照。

---

## 2. 目录结构 (目标 Layout)

> 以下为规划的目标目录，标记 ✅ 表示已落地，⏳ 表示待创建。

```
E:\SE mod\
├── MEMORY.md            ✅ 记忆 (跨会话事实)
├── CLAUDE.md            ✅ 编码规约
├── DATA.md              ✅ 结构/实体 (本文件)
│
├── CMakeLists.txt       ✅ 顶层 CMake (FetchContent 引入 CommonLibSSE-NG; VS gen + v145)
├── .gitignore           ✅ 忽略 build/.vcpkg/thirdparty 等
│
├── .vcpkg\              (保留无用的 vcpkg 克隆 — 已弃用, 依赖不走它)
├── thirdparty\          ✅ 依赖源码 (git clone, 不入库)
│   ├── spdlog\          ✅ spdlog v1.14.1
│   ├── CommonLibSSE-NG\ ✅ CommonLibSSE-NG (main/3.6.0)
│   └── rapidcsv\        ✅ rapidcsv
│
├── scripts\             ✅ 构建/打包脚本
│   └── build_spdlog.ps1 ✅ spdlog 预 build+install 到 build/deps/spdlog
│
├── src\                 ✅ 源码主目录
│   ├── main.cpp         ✅ 插件入口 (SKSEPlugin_Load) — kDataLoaded 注册 SecondSlotManager
│   ├── Version.cpp      ✅ SKSE 插件元数据/声明 (SKSEPluginInfo 宏, SKSE 加载必需)
│   ├── SecondSlotManager.h/.cpp ✅ X 键第二槽管理器 (记录 + 施放: 临时换槽->AddCastPower->还原)
│   └── MenuMouseIntercept.h/.cpp ✅ 魔法菜单纯查询工具 (IsZCastForm + GetSpellFromForm + ResolveHighlightedForm)
│   ├── skse\            ⏳ SKSE 相关封装 (Messaging, Settings)
│   ├── services\        ⏳ 各业务 Service 类
│   ├── hooks\           ⏳ hook 注册处 (分发到具体 hook)
│   ├── ui\              ⏳ (可选) 界面/菜单相关
│   └── util\            ⏳ 工具函数
│
├── include\             ⏳ (可选) 公共头文件
├── resources\           ⏳ (可选) 打包资源 (INI/SWF/ESP 生成物)
└── build\               ✅ CMake 构建输出(不入库, gitignore)
    ├── out\Release\     ✅ 插件 DLL 最终产物: SEMod.dll
    └── deps\spdlog\     ✅ spdlog 预安装目录 (build_spdlog.ps1 产物)
```

---

## 3. 关键文件职责表

| 文件/模块 | 职责 |
|-----------|------|
| `src/SecondSlotManager.{h,cpp}` | X 键第二槽管理器(Service)。持有第二槽 `TESForm*`；继承 `BSTEventSink<InputEvent*>` 挂 `BSInputDeviceManager`：X 键按下瞬间按**物理扫描码** `GetIDCode()==DIK_X` 判定(不靠 QUserEvent，曾因空命令名失效)，按 `UI::IsMenuOpen(MagicMenu)` 分化——菜单开=`StoreHighlightedToSlot()`(经 `MenuMouseIntercept` 读高亮项并判定 Z 键类后存槽)，菜单关=`CastSecondSlot()`(**经 `GetMagicCaster(kInstant)->CastSpellImmediate` 立即施放**)。**已实测可用** |
| `src/MenuMouseIntercept.{h,cpp}` | 魔法菜单纯查询工具(无 hook)：`IsZCastForm`(Shout 恒真；Spell 的 kPower/kLesserPower/kVoicePower)、`GetShoutFromForm`/`GetSpellFromForm`(解析可施放 spell)、`ResolveHighlightedForm`(读当前高亮项 formID→TESForm。实测路径: `Menu_mc.inventoryLists.panelContainer.itemList` → `itemList.selectedIndex` → `entryList[选中]` → **entry.formId**(小写d/number，无 data 层) → `LookupByID`)。供 SecondSlotManager 使用。**已实测可用** |
| `src/main.cpp` | 插件入口；`SKSEPlugin_Load` 做 `SKSE::Init` + 消息监听；`kDataLoaded` 注册 `SecondSlotManager`。**已编译通过** |
| `src/Version.cpp` | SKSE 插件元数据/声明(`SKSEPluginInfo` 宏导出 `SKSEPlugin_Version`/`SKSEPlugin_Query`，RuntimeCompatibility=AddressLibrary)；无此文件 SKSE 不加载插件。**已编译通过** |
| `src/skse/` | 封装 SKSE API：版本校验、消息分发、设置(Settings)读写 |
| `src/services/` | 业务逻辑单元，各自负责一个功能点，逻辑应与入口解耦 |
| `src/hooks/` | 集中注册 hook，把游戏事件转发到对应 Service |
| `src/util/` | 无业务状态的通用工具 |
| `scripts/build_spdlog.ps1` | 预 build+install spdlog 到 `build/deps/spdlog`（CommonLib 依赖 `find_package(spdlog CONFIG)`） |
| `CMakeLists.txt` | 构建定义；`add_library(MODULE)`；`/bigobj`；x64；**VS gen + v145**；输出到 `build/out/<config>/` |

---

## 4. 实体清单 (Entity Index)

> 当前处于骨架阶段，尚无源码实体。**一旦开始写代码，需在此登记每种实体**(类、结构体、游戏 Form、脚本变量等)。按需复制下列模板行。

### 4.1 代码实体 (类型 / 类)

| 实体 | 层次(服务/Hook/工具) | 描述 | 状态 |
|------|----------------------|------|------|
| `SecondSlotManager` | services | X 键第二槽：监听 X 键(BSTEventSink<InputEvent*>，按**物理码** GetIDCode()==DIK_X 判定)；场景分化——魔法菜单开=存高亮项、关=施放。施放经 **`GetMagicCaster(kInstant)->CastSpellImmediate`** 立即施放。持有 `m_secondSlot`(TESForm*)。**已实测可用** | ✅ |
| `MenuMouseIntercept` | 工具 | 纯查询、无 hook：`IsZCastForm`(Shout 恒真；Spell 的 kPower/kLesserPower/kVoicePower)、`GetShoutFromForm`/`GetSpellFromForm`(龙吼取 variations[0].spell)、`ResolveHighlightedForm`(读高亮项 formID→TESForm；实测字段=**entry.formId**，路径 `itemList.selectedIndex`→`entryList[选中]`)。**已实测可用** | ✅ |

### 4.2 游戏实体 (Form / Record / 对象)

> 涉及游戏内对象(Form 类型、EditorID、FormID、BaseID)在这里登记。SKSE 插件通常不暴露 ESP，但若涉及修改/读取 Form，用表格记录。

| FormID / EditorID | 类型 | 用途 | 状态 |
|-------------------|------|------|------|
| (待填) | (如 Actor/Quest/Keyword) | (用途) | ⏳ |

### 4.3 配置 / 数据文件

| 文件 | 存放路径 | 用途 | 状态 |
|------|----------|------|------|
| (待填, 如 `config.ini`) | `Data\SKSE\Plugins\` | (配置项说明) | ⏳ |

---

## 5. 运行期文件位置 (部署)

- 插件 DLL → `E:\SE mod\build\out\Release\SEMod.dll` → 部署到游戏 `Data\SKSE\Plugins\SEMod.dll`
- 日志 → `%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\SEMod.log`（本插件由 `SKSE::log` 生成）
- 需求插件：SKSE64、Address Library(适配 1.6.1170 的数据)

---

## 6. 维护约定

- 新建/删除文件、增加实体时，**必须更新本节清单**，保持与磁盘一致。
- 本文件只做「登记」，不做「决定」；结构性决策写进 `MEMORY.md` 的决定日志。
- 状态标记：✅ 已落地 / ⏳ 规划中 / 🔄 变更中。
