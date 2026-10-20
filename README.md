# Another Key for Shout (AKFS) — 源码包

Skyrim SE 1.6.1170 (AE) SKSE 插件源码。
X 键第二槽：把龙吼 / 特殊能力绑到独立按键，免去频繁开菜单切换。

---

## 快速开始

**先读 [打包说明.md](打包说明.md)** —— 包含完整的环境要求、依赖获取、构建步骤与踩坑清单。

最短路径：

1. 自行 `git clone` 三个依赖到 `thirdparty/`（CommonLibSSE-NG / spdlog / rapidcsv）
2. 预构建安装 spdlog（`打包说明.md` 第二节）
3. 配置并编译：
   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T v145
   cmake --build build --config Release
   ```
4. 产物 `build/out/Release/AKFS.dll`

---

## 目录说明

| 路径 | 内容 |
|---|---|
| `src/` | 全部 C++ 源码（见下方模块表） |
| `docs/` | 项目文档 |
| `plugin/` | 可直接部署的发行文件（DLL + 双语 ini） |
| `CMakeLists.txt` | 构建定义 |
| `打包说明.md` | **构建与部署完整说明** |
| `CHANGELOG.md` | 版本变更（中文） |
| `RELEASE_NOTES_EN.md` | 版本变更（英文） |

## 源码模块

| 文件 | 职责 |
|---|---|
| `main.cpp` | 插件入口 `SKSEPlugin_Load`：初始化日志、加载配置、注册菜单、注册消息监听 |
| `Version.cpp` | SKSE 插件元数据（`SKSEPluginInfo` 宏）。**缺少此文件 SKSE 不会加载插件** |
| `SecondSlotManager.{h,cpp}` | 核心：X 键监听、存槽、施放、冷却、原版龙吼通道注入 |
| `MenuMouseIntercept.{h,cpp}` | 读取魔法菜单 / 收藏菜单的当前高亮项 |
| `AKFSConfig.{h,cpp}` | `AKFS.ini` 读写、`[Strings]` 本地化、内存+ini 双写配置 |
| `AKFSMenu.{h,cpp}` | SKSE Menu Framework 界面（设置页 + 状态页，可选依赖） |

## 项目文档

| 文件 | 说明 |
|---|---|
| `docs/CLAUDE.md` | 编码规约与工作习惯 |
| `docs/MEMORY.md` | **决策日志与实测踩坑记录**（非常有价值，含大量引擎行为实测结论） |
| `docs/DATA.md` | 项目结构与实体清单 |
| `docs/Knowledge/SHOUT_CAST_MECHANISM.md` | 原生龙吼施放机制定稿 |
| `docs/Knowledge/INPUT_FUNCTION_CHAIN.md` | 按键 → 施放 的引擎函数链实测分析 |

> `docs/MEMORY.md` 记录了大量逆向与实测结论（voice 通道、magic caster、
> 菜单 swf 字段路径等），二次开发时能省下大量试错成本。

---

## 运行时前置

| 前置 | 必需性 |
|---|---|
| SKSE64（对应 1.6.1170） | **必需** |
| Address Library for SKSE Plugins | **必需** |
| SKSE Menu Framework | 可选（仅配置菜单需要） |

## 部署

```powershell
Copy-Item "plugin/Another Key for Shout/AKFS.dll"   "<游戏>\Data\SKSE\Plugins\AKFS.dll" -Force
Copy-Item "plugin/Another Key for Shout/Language/Chinese/AKFS.ini" "<游戏>\Data\SKSE\Plugins\AKFS.ini" -Force
```

> ⚠️ DLL 与 ini 必须成对更新。旧 ini 无 `[Strings]` 节会导致菜单显示成键名。

---

## 版本

- 发布版本：**1.1.1**
- 内部迭代号：v1.12
- 详见 [CHANGELOG.md](CHANGELOG.md)
