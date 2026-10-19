// MenuMouseIntercept.cpp — 魔法菜单读取纯查询实现（v0.3，零 hook）
//
// 只做两类只读查询，不挂任何 hook / detour：
//   1. IsZCastForm(form) / GetSpellFromForm(form) —— 判断/解析 Z 键类表单。
//   2. ResolveHighlightedForm() —— 在魔法菜单里读出当前高亮项。
//
// 读取走 MagicMenu.swf 的 rootList/entryList，selectedIndex 直接下标（不依赖
// GetLength/length，实测 Flash 数组长度读取失败）。全程防御判错，读不到返回 nullptr。
#include "MenuMouseIntercept.h"

#include "SKSE/Impl/PCH.h"
#include "SKSE/Logger.h"
#include "RE/F/FormTraits.h"
#include "RE/F/FavoritesMenu.h"
#include "RE/G/GFxValue.h"
#include "RE/M/MagicMenu.h"
#include "RE/M/MagicSystem.h"
#include "RE/S/SpellItem.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESShout.h"
#include "RE/U/UI.h"

namespace SEMod
{
    namespace MenuMouseIntercept
    {
        bool IsZCastForm(const RE::TESForm* a_form)
        {
            if (!a_form)
                return false;
            if (a_form->Is(RE::FormType::Shout))
                return true;  // 龙吼恒为 Z 键类
            auto* spell = a_form->As<RE::SpellItem>();
            if (!spell)
                return false;
            switch (spell->GetSpellType()) {
                case RE::MagicSystem::SpellType::kPower:
                case RE::MagicSystem::SpellType::kLesserPower:
                case RE::MagicSystem::SpellType::kVoicePower:
                    return true;
                default:
                    return false;
            }
        }

        RE::SpellItem* GetSpellFromForm(RE::TESForm* a_form)
        {
            if (!a_form)
                return nullptr;
            if (auto* shout = a_form->As<RE::TESShout>())
                return shout->variations[RE::TESShout::VariationIDs::kOne].spell;
            return a_form->As<RE::SpellItem>();
        }

        RE::TESForm* ResolveHighlightedForm()
        {
            auto* ui = RE::UI::GetSingleton();
            auto menuPtr = ui ? ui->GetMenu<RE::MagicMenu>() : RE::GPtr<RE::MagicMenu>{};
            auto* menu = menuPtr.get();
            if (!menu)
                return nullptr;

            auto& rt = menu->GetRuntimeData();
            RE::GFxValue root = rt.root;  // "Menu_mc"
            SKSE::log::info("AKFS: root type={}", static_cast<int>(root.GetType()));
            if (!root.IsObject()) {
                SKSE::log::info("AKFS: root 不是 object，提前返回（MagicMenu 可能未就绪）");
                return nullptr;
            }

            // 路径 1：rootList.selectedIndex → getItemAt → .data.formID
            {
                RE::GFxValue rootList;
                if (!root.GetMember("rootList", &rootList) || !rootList.IsObject()) {
                    SKSE::log::info("AKFS: 读取路径1失败 - rootList 取不到");
                } else {
                    RE::GFxValue sel;
                    if (!rootList.GetMember("selectedIndex", &sel) || !sel.IsNumber()) {
                        SKSE::log::info("AKFS: 读取路径1失败 - rootList.selectedIndex 取不到");
                    } else {
                        const int idx = static_cast<int>(sel.GetNumber());
                        SKSE::log::info("AKFS: 读取路径1 rootList.selectedIndex={}", idx);
                        if (idx >= 0) {
                            RE::GFxValue args[1];
                            args[0].SetNumber(idx);
                            RE::GFxValue item;
                            if (rootList.Invoke("getItemAt", &item, args, 1) && item.IsObject()) {
                                RE::GFxValue data;
                                if (item.GetMember("data", &data)) {
                                    auto fid = FormIdFromValue(data);
                                    if (fid) {
                                        SKSE::log::info("AKFS: 读取路径1 命中 formID={:08X}", fid);
                                        return RE::TESForm::LookupByID(fid);
                                    }
                                }
                            }
                            SKSE::log::info("AKFS: 读取路径1 failed - getItemAt 或 data 取不到");
                        }
                    }
                }
            }

            // 路径 2：inventoryLists.panelContainer.itemList → entryList[selectedIndex].data.formID
            {
                RE::GFxValue invLists;
                if (!root.GetMember("inventoryLists", &invLists) || !invLists.IsObject()) {
                    SKSE::log::info("AKFS: 读取路径2失败 - inventoryLists 取不到");
                } else {
                    RE::GFxValue panel;
                    if (!invLists.GetMember("panelContainer", &panel) || !panel.IsObject()) {
                        SKSE::log::info("AKFS: 读取路径2失败 - panelContainer 取不到");
                    } else {
                        RE::GFxValue itemList;
                        if (!panel.GetMember("itemList", &itemList) || !itemList.IsObject()) {
                            SKSE::log::info("AKFS: 读取路径2失败 - itemList 取不到");
                        } else {
                            RE::GFxValue entryList;
                            if (!itemList.GetMember("entryList", &entryList) || !entryList.IsObject()) {
                                SKSE::log::info("AKFS: 读取路径2失败 - entryList 取不到");
                            } else {
                                RE::GFxValue sel;
                                // selectedIndex 属于 itemList，不是 entryList。entryList 只是数组成员，
                                // 没有 selectedIndex。MEMORY 结论：itemList(对象)含 entryList 数组成员，
                                // 选中索引在 itemList.selectedIndex，再 entryList[selectedIndex]。
                                if (!itemList.GetMember("selectedIndex", &sel) || !sel.IsNumber()) {
                                    SKSE::log::info("AKFS: 读取路径2失败 - itemList.selectedIndex 取不到");
                                } else {
                                    const int idx = static_cast<int>(sel.GetNumber());
                                    SKSE::log::info("AKFS: 读取路径2 entryList.selectedIndex={}", idx);
                                    if (idx >= 0) {
                                        RE::GFxValue entry;
                                        if (!entryList.GetElement(idx, &entry) || !entry.IsObject()) {
                                            SKSE::log::info("AKFS: 读取路径2失败 - GetElement({}) 取不到", idx);
                                        } else {
                                            // 实测：魔法菜单条目把表单 ID 挂在 entry 自身，字段名 "formId"（小写 d），
                                            // number 类型（entry 无 data 成员）。直接从 entry 解析。
                                            auto fid = FormIdFromValue(entry);
                                            if (fid) {
                                                SKSE::log::info("AKFS: 读取路径2 命中 formID={:08X}", fid);
                                                return RE::TESForm::LookupByID(fid);
                                            }
                                            SKSE::log::info("AKFS: 读取路径2 failed - entry 无 formId");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SKSE::log::info("AKFS: 读取魔法菜单高亮项失败（两条路径均未命中）");
            return nullptr;
        }

        RE::TESForm* ResolveFavoritesHighlightedForm()
        {
            auto* ui = RE::UI::GetSingleton();
            auto menuPtr = ui ? ui->GetMenu<RE::FavoritesMenu>() : RE::GPtr<RE::FavoritesMenu>{};
            auto* menu = menuPtr.get();
            if (!menu)
                return nullptr;

            auto& rt = menu->GetRuntimeData();
            const auto count = rt.favorites.size();

            // 选中索引：尝试从 swf root 读 selectedIndex（单列收藏列表常见字段）。
            std::int32_t selIdx = -1;
            RE::GFxValue sel;
            if (rt.root.IsObject() && rt.root.GetMember("selectedIndex", &sel) && sel.IsNumber()) {
                selIdx = static_cast<std::int32_t>(sel.GetNumber());
                SKSE::log::info("AKFS: [favorites] root.selectedIndex={}", selIdx);
            } else {
                SKSE::log::info("AKFS: [favorites] root.selectedIndex 取不到，退回遍历收藏列表");
            }

            // 优先：用选中索引取收藏项。
            if (selIdx >= 0 && selIdx < static_cast<std::int32_t>(count)) {
                auto* form = rt.favorites[static_cast<std::uint32_t>(selIdx)].item;
                if (form) {
                    SKSE::log::info("AKFS: [favorites] 命中收藏项 index={} FormID={:08X}",
                                    selIdx, form->GetFormID());
                    return form;
                }
                SKSE::log::info("AKFS: [favorites] 收藏项为空，继续查找");
            }

            // 兜底：遍历整个收藏列表，返回第一个 Z 键类项。
            for (std::uint32_t i = 0; i < count; ++i) {
                auto* form = rt.favorites[i].item;
                if (!form)
                    continue;
                if (IsZCastForm(form)) {
                    SKSE::log::info("AKFS: [favorites] 兜底取第一个 Z 键类项 index={} FormID={:08X}",
                                    i, form->GetFormID());
                    return form;
                }
            }

            SKSE::log::info("AKFS: 读取收藏菜单高亮项失败（无收藏或无可施放项）");
            return nullptr;
        }

        std::uint32_t FormIdFromValue(const RE::GFxValue& a_data)
        {
            // 实测魔法菜单条目：formID 直接挂在 entry 上，字段名是 "formId"（小写 d），number 类型。
            // 同时也兼容旧 "formID" 字段名与 hex 字符串形式。
            for (const char* key : { "formId", "formID" }) {
                RE::GFxValue fid;
                if (a_data.GetMember(key, &fid)) {
                    if (fid.IsNumber())
                        return static_cast<std::uint32_t>(fid.GetNumber());
                    if (fid.IsString()) {
                        const char* s = fid.GetString();
                        if (s && *s) {
                            std::string_view sv(s);
                            if (sv.size() >= 8)
                                sv = sv.substr(sv.size() - 8);
                            char* end = nullptr;
                            const auto v = std::strtoul(sv.data(), &end, 16);
                            if (end && *end == '\0')
                                return static_cast<std::uint32_t>(v);
                        }
                    }
                }
            }
            return 0;
        }
    }
}