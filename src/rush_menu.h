#pragma once
// The menus the ghost of the graveyard shows after 強者たちと戦う: which items each menu has,
// and what each item does. Pure data; mod.cpp shows them through the ModLoader.
#include "unlocks.h"
#include <wots4/mod_api.h>

namespace boss_rush {

enum class RushGroup : uint8_t {
    joui,          // 小暮, まだら, 赤城
    shogunate,     // 茂呂, 琴吹, 鬼怒川三姉妹, 鬼怒川, 鬼怒川 (boiled)
    foreigners,    // メリンダ, the new captain, J.J
    shitenno,      // 虎鉄, 大蛇, 熊神, 霧鮫
    yoningumi,     // 鈴目, 緋横, 小鳩, 百舌
    amihamaEight,  // the boss, the Shitenno, the Yoningumi, the commander
};

enum class RushMenu : uint8_t {
    sides,    // The three factions or the town
    factions,
    town,
};

struct RushMenuItem {
    wots4::LocalizedText label;
    bool opensMenu;  // true: opens menu, false: starts group
    RushMenu menu;
    RushGroup group;
};

constexpr uint32_t maxRushMenuItems = 3;

struct RushMenuItems {
    RushMenuItem items[maxRushMenuItems];
    uint32_t count = 0;

    void add(const RushMenuItem& item) { items[count++] = item; }
};

constexpr wots4::LocalizedText fightTheStrongLabel{L"強者たちと戦う", L"Fight the strong"};

// The first menu: without a cleared town occupation, the town has no one to fight.
inline RushMenu firstMenu(const Unlocks& unlocks) {
    return unlocks.shitennoVsYoningumi || unlocks.amihamaEight ? RushMenu::sides : RushMenu::factions;
}

inline RushMenuItem opens(wots4::LocalizedText label, RushMenu menu) {
    return {label, true, menu, RushGroup::joui};
}

inline RushMenuItem starts(wots4::LocalizedText label, RushGroup group) {
    return {label, false, RushMenu::sides, group};
}

inline RushMenuItems menuItems(RushMenu menu, const Unlocks& unlocks) {
    RushMenuItems items;
    switch (menu) {
    case RushMenu::sides:
        items.add(opens({L"三勢力の強者と戦う", L"Fight the strong of the three factions"}, RushMenu::factions));
        items.add(opens({L"町の強者と戦う", L"Fight the strong of the town"}, RushMenu::town));
        break;
    case RushMenu::factions:
        items.add(starts({L"攘夷の強者と戦う", L"Fight the strong of the Joui"}, RushGroup::joui));
        items.add(starts({L"幕府の強者と戦う", L"Fight the strong of the Shogunate"}, RushGroup::shogunate));
        items.add(starts({L"外国の強者と戦う", L"Fight the strong of the foreigners"}, RushGroup::foreigners));
        break;
    case RushMenu::town:
        if (unlocks.shitennoVsYoningumi) {
            items.add(starts({L"四天王と戦う", L"Fight the Shitenno"}, RushGroup::shitenno));
            items.add(starts({L"四人組と戦う", L"Fight the Yoningumi"}, RushGroup::yoningumi));
        }
        if (unlocks.amihamaEight) items.add(starts({L"八人衆と戦う", L"Fight the Amihama Eight"}, RushGroup::amihamaEight));
        break;
    }
    return items;
}

inline const char* groupName(RushGroup group) {
    switch (group) {
    case RushGroup::joui: return "Joui";
    case RushGroup::shogunate: return "Shogunate";
    case RushGroup::foreigners: return "foreigners";
    case RushGroup::shitenno: return "Shitenno";
    case RushGroup::yoningumi: return "Yoningumi";
    case RushGroup::amihamaEight: return "Amihama Eight";
    }
    return "unknown";
}

}  // namespace boss_rush
