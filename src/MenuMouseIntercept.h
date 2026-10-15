// MenuMouseIntercept.h — 魔法菜单读取纯查询工具（v0.3 重写，无任何 hook）
//
// 只做两类只读查询，不给游戏挂任何 detour / vtable hook：
//   - IsZCastForm / GetSpellFromForm：判定并解析"Z 键类"（龙吼或力量/小力/龙吼之声法术）。
//   - ResolveHighlightedForm：读魔法菜单当前高亮项的 formID → TESForm。
//     读取路径依赖 Flash(swf) 数据；Flash 数组 GetLength/length 实测读不到，
//     因此以已知 selectedIndex 直接下标读等价物，绝不依赖长度。
#pragma once

#include "SKSE/Impl/PCH.h"

namespace RE
{
    class TESForm;
    class SpellItem;
    struct GFxValue;
}

namespace SEMod::MenuMouseIntercept
{
    // 是否属于 Z 键可施放类别：龙吼(FormType::Shout)恒真；
    // SpellItem 的 kPower / kLesserPower / kVoicePower 为真，其余法术为假。
    bool IsZCastForm(const RE::TESForm* a_form);

    // 给定已判定为 Z 键类的表单，解析出可施放法术：
    //   龙吼 → variations[TESShout::VariationIDs::kOne].spell；
    //   其它 Power 类法术 → 直接 As<SpellItem>()。
    RE::SpellItem* GetSpellFromForm(RE::TESForm* a_form);

    // 读魔法菜单当前高亮项的表单；读不到返回 nullptr。
    // 两道路径：rootList+getItemAt+data.formID，或
    // inventoryLists.panelContainer.itemList → entryList[selectedIndex] → data.formID。
    // 全程防御判错，读不到返回 nullptr（不抛异常）。
    RE::TESForm* ResolveHighlightedForm();

    // 读收藏菜单(FavoritesMenu，Q 键弹出的侧菜单)当前高亮项的表单。
    // 收藏项直接存于 C++ 侧 rt.favorites(BSTArray<Entry>，Entry.item=TESForm*)；
    // 选中索引从 swf root 的 selectedIndex 读（防御）：读不到或越界时退回
    // "整个收藏列表第一个 Z 键类项"。全程防御判错，读不到返回 nullptr。
    RE::TESForm* ResolveFavoritesHighlightedForm();

    // 实现细节：从条目 data 里取出 formID（Number 或 8 位 hex 字符串），解析失败返回 0。
    std::uint32_t FormIdFromValue(RE::GFxValue const& a_data);
}
