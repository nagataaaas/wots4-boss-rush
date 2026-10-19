// Checks the unlocks and the menus against a synthetic save layout. No game exe is used.
#include "rush_roster.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace boss_rush;

namespace {

unsigned tests, failures;

void check(const char* name, bool ok) {
    ++tests;
    if (!ok) ++failures;
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
}

constexpr size_t saveSize = 0x4000;

std::vector<uint8_t> emptySave() {
    std::vector<uint8_t> save(saveSize, 0);
    // Occupations nobody offered yet
    for (uint32_t id = 0; id < 256; ++id) memset(save.data() + occupationClears + id * 2, 0xff, 2);
    return save;
}

void setClears(std::vector<uint8_t>& save, uint32_t occupation, uint16_t clears) {
    memcpy(save.data() + occupationClears + occupation * 2, &clears, 2);
}

std::wstring labels(const RushMenuItems& items) {
    std::wstring text;
    for (uint32_t i = 0; i < items.count; ++i) {
        if (i) text += L'|';
        text += items.items[i].label.japanese;
    }
    return text;
}

void checkUnlocks() {
    std::vector<uint8_t> save = emptySave();
    Unlocks unlocks = readUnlocks(save.data());
    check("a new game has nothing unlocked", !unlocks.lightOfHope && !unlocks.shitennoVsYoningumi && !unlocks.amihamaEight);
    save[eventRecords + lightOfHopeEvent] = eventSucceeded;
    check("the ending in the records unlocks the line", readUnlocks(save.data()).lightOfHope);
    save[eventRecords + lightOfHopeEvent] = 5;
    check("a failed record does not", !readUnlocks(save.data()).lightOfHope);
    save[eventStates + lightOfHopeEvent] = eventSucceeded;
    check("the ending in this playthrough does", readUnlocks(save.data()).lightOfHope);

    setClears(save, shitennoVsYoningumiOccupation, 0);
    check("an offered but uncleared occupation is not cleared", !readUnlocks(save.data()).shitennoVsYoningumi);
    setClears(save, shitennoVsYoningumiOccupation, 2);
    setClears(save, amihamaEightOccupation, 1);
    unlocks = readUnlocks(save.data());
    check("cleared occupations count", unlocks.shitennoVsYoningumi && unlocks.amihamaEight);
}

void checkMenus() {
    Unlocks none;
    none.lightOfHope = true;
    check("without town occupations, the factions come first", firstMenu(none) == RushMenu::factions);
    check("the factions menu", labels(menuItems(RushMenu::factions, none)) == L"攘夷の強者と戦う|幕府の強者と戦う|外国の強者と戦う");
    const RushMenuItems factions = menuItems(RushMenu::factions, none);
    check("the factions start their groups", !factions.items[1].opensMenu && factions.items[1].group == RushGroup::shogunate);

    Unlocks shitenno = none;
    shitenno.shitennoVsYoningumi = true;
    check("with 四天王 対 四人組, the sides come first", firstMenu(shitenno) == RushMenu::sides);
    const RushMenuItems sides = menuItems(RushMenu::sides, shitenno);
    check("the sides open the factions and the town", sides.count == 2 && sides.items[0].opensMenu &&
        sides.items[0].menu == RushMenu::factions && sides.items[1].menu == RushMenu::town);
    check("the town after 四天王 対 四人組", labels(menuItems(RushMenu::town, shitenno)) == L"四天王(熊神除く)と戦う|四人組と戦う");

    Unlocks eight = none;
    eight.amihamaEight = true;
    check("with あみはま八人衆 only, the sides come first", firstMenu(eight) == RushMenu::sides);
    check("the town after あみはま八人衆", labels(menuItems(RushMenu::town, eight)) == L"八人衆と戦う");
    eight.shitennoVsYoningumi = true;
    const RushMenuItems town = menuItems(RushMenu::town, eight);
    check("the town after both", labels(town) == L"四天王(熊神除く)と戦う|四人組と戦う|八人衆と戦う" &&
        town.items[2].group == RushGroup::amihamaEight);
    bool labelsFit = true;
    for (RushMenu menu : {RushMenu::sides, RushMenu::factions, RushMenu::town}) {
        const RushMenuItems items = menuItems(menu, eight);
        for (uint32_t i = 0; i < items.count; ++i) {
            labelsFit &= wcslen(items.items[i].label.english) <= wots4::maxConversationTextChars &&
                wcslen(items.items[i].label.japanese) <= wots4::maxConversationTextChars;
        }
    }
    check("every label fits a conversation line", labelsFit);
}

uint32_t fightersOf(RushGroup group) {
    const RushRoster roster = rosterOf(group);
    uint32_t count = 0;
    for (uint32_t i = 0; i < roster.stageCount; ++i) count += roster.stages[i].fighterCount;
    return count;
}

void checkRosters() {
    check("the Joui: 小暮, まだら, 赤城", rosterOf(RushGroup::joui).stageCount == 3 && rosterOf(RushGroup::joui).stages[0].definitions[0] == 69);
    const RushRoster shogunate = rosterOf(RushGroup::shogunate);
    check("the Shogunate: 5 stages, the sisters together under one name",
        shogunate.stageCount == 5 && shogunate.stages[2].fighterCount == 3 &&
            std::wstring(shogunate.stages[2].title.japanese) == L"鬼怒川三姉妹");
    check("the armed forms: 茂呂 640, the sisters 445-447, the boiled 鬼怒川 452",
        shogunate.stages[0].definitions[0] == 640 && shogunate.stages[2].definitions[0] == 445 &&
            shogunate.stages[2].definitions[2] == 447 && shogunate.stages[4].definitions[0] == 452);
    check("the new commander in armor (522)", rosterOf(RushGroup::foreigners).stages[1].definitions[0] == 522);
    check("the foreigners end with J.J", rosterOf(RushGroup::foreigners).stages[2].definitions[0] == 44);
    check("the Shitenno and the Yoningumi fight one by one",
        fightersOf(RushGroup::shitenno) == 3 && rosterOf(RushGroup::shitenno).stageCount == 3 &&
            fightersOf(RushGroup::yoningumi) == 4 && rosterOf(RushGroup::yoningumi).stageCount == 4);
    constexpr uint32_t kumagami = 338;
    const RushRoster shitenno = rosterOf(RushGroup::shitenno);
    bool shitennoHasKumagami = false;
    for (uint32_t i = 0; i < shitenno.stageCount; ++i) shitennoHasKumagami |= shitenno.stages[i].definitions[0] == kumagami;
    check("the Shitenno without 熊神: 虎鉄, 大蛇, 霧鮫", !shitennoHasKumagami && shitenno.stages[0].definitions[0] == 330 &&
        shitenno.stages[1].definitions[0] == 331 && shitenno.stages[2].definitions[0] == 329);
    const RushRoster eight = rosterOf(RushGroup::amihamaEight);
    check("熊神 still fights among the Amihama Eight", eight.stages[1].definitions[2] == kumagami);
    check("the Amihama Eight: the boss, 4 together, 4 together, the commander",
        eight.stageCount == 4 && eight.stages[0].definitions[0] == 286 && eight.stages[1].fighterCount == 4 &&
            eight.stages[2].fighterCount == 4 && eight.stages[3].definitions[0] == 337);
    bool fits = true;
    for (RushGroup group : {RushGroup::joui, RushGroup::shogunate, RushGroup::foreigners, RushGroup::shitenno,
             RushGroup::yoningumi, RushGroup::amihamaEight}) {
        const RushRoster roster = rosterOf(group);
        fits &= roster.stageCount >= 1 && roster.stageCount <= maxStages;
        for (uint32_t i = 0; i < roster.stageCount; ++i) {
            const RushStage& stage = roster.stages[i];
            fits &= stage.fighterCount >= 1 && stage.fighterCount <= maxStageFighters;
            // A stage of several fighters is announced by its own name, not one fighter's.
            fits &= stage.fighterCount == 1 || stage.title.japanese != nullptr;
            fits &= (stage.title.japanese != nullptr) == (stage.title.english != nullptr);
        }
    }
    check("every stage has 1 to 4 fighters and a name when it has several", fits);
}

}  // namespace

int main() {
    checkUnlocks();
    checkMenus();
    checkRosters();
    printf("RESULT %u passed, %u failed (synthetic save layout only; no game exe, no save, no game process)\n",
        tests - failures, failures);
    return failures ? 1 : 0;
}
