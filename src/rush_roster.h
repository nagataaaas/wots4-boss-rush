#pragma once
// Who each group fights, stage by stage. Definition numbers are rows of the game's AcNPCDefine;
// each is the one that comes armed, in its fighting clothes.
#include "rush_menu.h"

namespace boss_rush {

constexpr uint32_t maxStageFighters = 4;
constexpr uint32_t maxStages = 5;

struct RushStage {
    uint32_t definitions[maxStageFighters];
    uint32_t fighterCount;
    // Announced when the stage starts. nullptr texts: the game's name of the only fighter.
    wots4::LocalizedText title;
};

struct RushRoster {
    RushStage stages[maxStages];
    uint32_t stageCount;
};

// Full names as the game's appearance list has them.
constexpr wots4::LocalizedText fightersName{nullptr, nullptr};
constexpr wots4::LocalizedText kogure{L"小暮 迅雷", L"Jinrai Kogure"};
constexpr wots4::LocalizedText madara{L"まだら", L"Madara"};
constexpr wots4::LocalizedText akagi{L"赤城 烈斗", L"Reddo Akagi"};
constexpr wots4::LocalizedText moro{L"茂呂 茂", L"Shigeru Moro"};
constexpr wots4::LocalizedText kotobuki{L"琴吹 光", L"Hikaru Kotobuki"};
constexpr wots4::LocalizedText kinugawaSisters{L"鬼怒川三姉妹", L"The Kinugawa Sisters"};
constexpr wots4::LocalizedText kinugawa{L"鬼怒川 怨仙", L"Onsen Kinugawa"};
constexpr wots4::LocalizedText melinda{L"メリンダ", L"Melinda"};
constexpr wots4::LocalizedText newCommander{L"新隊長", L"New Commander"};
constexpr wots4::LocalizedText jj{L"J.J", L"J.J."};
constexpr wots4::LocalizedText shitenno{L"四天王", L"The Shitenno"};
constexpr wots4::LocalizedText yoningumi{L"四人組", L"The Yoningumi"};

// Definitions: 小暮 69, まだら 70, 赤城 46, 茂呂 640 (with a sword; 65 fights bare-handed),
// 琴吹 64, 万由 445, 千佳 446, 百合 447 (in fighting clothes; 66/87/88 wear kimonos), 鬼怒川 67,
// 鬼怒川 boiled 452, メリンダ 68, 新隊長 522 (in armor; 153 in a dress), J.J 44, 虎鉄 330,
// 大蛇 331, 熊神 338, 霧鮫 329, 鈴目 334, 緋横 333, 小鳩 332, 百舌 335, 親分 286, 指揮官 337.
inline RushRoster rosterOf(RushGroup group) {
    switch (group) {
    case RushGroup::joui:
        return {{{{69}, 1, kogure}, {{70}, 1, madara}, {{46}, 1, akagi}}, 3};
    case RushGroup::shogunate:
        return {{{{640}, 1, moro}, {{64}, 1, kotobuki}, {{445, 446, 447}, 3, kinugawaSisters}, {{67}, 1, kinugawa},
                    {{452}, 1, kinugawa}},
            5};
    case RushGroup::foreigners:
        return {{{{68}, 1, melinda}, {{522}, 1, newCommander}, {{44}, 1, jj}}, 3};
    case RushGroup::shitenno:
        return {{{{330}, 1, fightersName}, {{331}, 1, fightersName}, {{338}, 1, fightersName}, {{329}, 1, fightersName}}, 4};
    case RushGroup::yoningumi:
        return {{{{334}, 1, fightersName}, {{333}, 1, fightersName}, {{332}, 1, fightersName}, {{335}, 1, fightersName}}, 4};
    case RushGroup::amihamaEight:
        return {{{{286}, 1, fightersName}, {{330, 331, 338, 329}, 4, shitenno}, {{334, 333, 332, 335}, 4, yoningumi},
                    {{337}, 1, fightersName}},
            4};
    }
    return {{}, 0};
}

}  // namespace boss_rush
