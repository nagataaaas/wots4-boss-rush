#include "rush.h"
#include "exit_lock.h"
#include "game_actors.h"
#include "travel.h"
#include <cstdio>
#include <cstring>

namespace boss_rush {
namespace {

constexpr ULONGLONG restBetweenStagesMs = 3000;
constexpr ULONGLONG loadingTimeoutMs = 20000;  // Longer than any model load seen in the game
// A corpse of a boss stays for the player to search, until the same boss comes in again; the
// game brings in no second actor with the ID of one that is there (0x43a760).
constexpr ULONGLONG corpseRemovalMs = 300;
// The empty graveyard: the map change and its loading. The first stage already waits in the
// graveyard as the screen comes back, a moment after the player can move (the area's characters
// are rebuilt then).
constexpr ULONGLONG travelTimeoutMs = 30000;
// The map change blacks the screen out at once, so a notice comes first.
constexpr ULONGLONG noticeBeforeTravelMs = 1500;
constexpr wots4::LocalizedText travelNotice{L"強者たちが墓場で待っている……", L"The strong await you in the graveyard..."};
// While the game brings an actor in, its parameters may be missing for a moment.
constexpr ULONGLONG missingParamsLimitMs = 3000;
// After the player falls, the bosses are kept off the body at most this long.
constexpr ULONGLONG fallenPlayerWatchMs = 6000;
// A boss that finds no way to the player would keep the exits locked for good: when no standing
// boss comes this near the player for this long, the stage gives up. The bosses come in 50 to
// 110 units away and fight within reach of a few units.
constexpr float reachDistance = 30.0f;
constexpr ULONGLONG unreachedLimitMs = 60000;

constexpr wots4::LocalizedText allDefeated{L"強者たちをすべて倒した", L"You have defeated all the strong."};
constexpr wots4::LocalizedText bossesGone{L"強者たちは姿を消した……", L"The strong have vanished..."};

enum class Phase { idle, announcing, travelling, clearing, loading, fighting, resting };

struct Fighter {
    int32_t actor;
    uint8_t* param;  // Once loaded
    bool down;
    float position[3];  // Where it comes in
    ULONGLONG missingSince;  // When its parameters went missing, or 0
    bool sheathSettled;
    int32_t groupBefore;  // Its battle group before the rush set it, put back when the player falls
};

struct Rush {
    Phase phase = Phase::idle;
    RushGroup group = RushGroup::joui;
    RushRoster roster{};
    uint32_t stage = 0;
    Fighter fighters[maxStageFighters]{};
    uint32_t fighterCount = 0;
    ULONGLONG phaseStart = 0;
    int32_t area = -1;
    int32_t playerGroupBefore = -1;  // The player's battle group, put back when the rush ends
    ULONGLONG lastReachedAt = 0;     // When a standing boss was last within reachDistance
    uint32_t fallenWatchCount = 0;  // The fighters kept off the player's body after the fall
    ULONGLONG fallenWatchUntil = 0;
};

Rush rush;
LogFunction log = nullptr;

void logLine(const char* message) {
    if (log) log(message);
}

void enterPhase(Phase phase) {
    rush.phase = phase;
    rush.phaseStart = GetTickCount64();
}

ULONGLONG inPhaseMs() {
    return GetTickCount64() - rush.phaseStart;
}

const wchar_t* inGameLanguage(const wots4::LocalizedText& text) {
    return game::textsInJapanese() ? text.japanese : text.english;
}

bool withinReach(const uint8_t* boss, const uint8_t* player) {
    const float* bossPosition = game::position(boss);
    const float* playerPosition = game::position(player);
    const float dx = bossPosition[0] - playerPosition[0];
    const float dy = bossPosition[1] - playerPosition[1];
    const float dz = bossPosition[2] - playerPosition[2];
    return dx * dx + dy * dy + dz * dz <= reachDistance * reachDistance;
}

// Each frame from the player's fall until the game takes the body away: the game sets a boss on
// the fallen player again (its target choice 0x440440 skips only flags 0x3000000, which come
// later), and it would keep cutting the body.
void keepOffFallenPlayer() {
    if (rush.fallenWatchCount == 0) return;
    const uint8_t* player = game::playerParam();
    if (!player || !game::fallen(player) || GetTickCount64() >= rush.fallenWatchUntil) {
        rush.fallenWatchCount = 0;
        return;
    }
    for (uint32_t i = 0; i < rush.fallenWatchCount; ++i) {
        Fighter& fighter = rush.fighters[i];
        // Looked up again: the bodies of the beaten ones may be gone.
        fighter.param = game::actorParam(fighter.actor);
        if (!fighter.param || game::defeated(fighter.param)) continue;
        game::leaveFight(fighter.param);
    }
}

void removeFighters() {
    for (uint32_t i = 0; i < rush.fighterCount; ++i) game::removeActor(rush.fighters[i].actor);
    rush.fighterCount = 0;
}

// Removes the bodies of the stage's bosses left from before. True if there was one.
bool removeStageCorpses() {
    const RushStage& stage = rush.roster.stages[rush.stage];
    bool removed = false;
    for (uint32_t i = 0; i < stage.fighterCount; ++i) {
        const int32_t actor = game::actorIdOf(stage.definitions[i]);
        if (actor == game::noActor || !game::actorParam(actor)) continue;
        game::removeActor(actor);
        removed = true;
        char message[112];
        sprintf_s(message, "[INFO] Removed the body of actor %d before definition %u comes in again.", actor,
            stage.definitions[i]);
        logLine(message);
    }
    return removed;
}

void endRush(const char* reason, bool removeThem, bool won = false) {
    if (removeThem) removeFighters();
    if (uint8_t* player = game::playerParam()) game::setBattleGroup(player, rush.playerGroupBefore);
    setExitsLocked(false);
    endEmptyGraveyard(won);
    rush.fighterCount = 0;
    game::setInflowStopped(false);
    enterPhase(Phase::idle);
    logLine(reason);
}

bool queueStage() {
    const RushStage& stage = rush.roster.stages[rush.stage];
    rush.fighterCount = 0;
    // The fighters take the entrances' sides in turn.
    float entrances[maxStageFighters][3];
    const uint32_t entranceCount = game::listEntrances(entrances, maxStageFighters);
    for (uint32_t i = 0; i < stage.fighterCount; ++i) {
        float position[3];
        float taken[maxStageFighters][3];
        for (uint32_t j = 0; j < rush.fighterCount; ++j) memcpy(taken[j], rush.fighters[j].position, sizeof(taken[j]));
        const float* side = entranceCount > 0 ? entrances[i % entranceCount] : nullptr;
        const bool placed = game::pickSpawnPoint(side, position, taken, rush.fighterCount);
        const int32_t actor = placed ? game::queueActor(stage.definitions[i], position) : game::noActor;
        if (actor == game::noActor) {
            char message[160];
            sprintf_s(message, "[ERROR] The game did not bring in definition %u for stage %u; the rush stops.",
                stage.definitions[i], rush.stage + 1);
            endRush(message, true);
            return false;
        }
        rush.fighters[rush.fighterCount++] = {actor, nullptr, false, {position[0], position[1], position[2]}, 0};
    }
    enterPhase(Phase::loading);
    char message[128];
    sprintf_s(message, "[INFO] Stage %u of the %s: %u fighter(s) are coming in.", rush.stage + 1, groupName(rush.group),
        stage.fighterCount);
    logLine(message);
    return true;
}

// The removal takes effect before the next stage's bosses are asked for.
bool bringInStage() {
    rush.fighterCount = 0;
    if (!removeStageCorpses()) return queueStage();
    enterPhase(Phase::clearing);
    return true;
}

void updateClearing() {
    if (inPhaseMs() >= corpseRemovalMs) queueStage();
}

void updateAnnouncing() {
    if (inPhaseMs() < noticeBeforeTravelMs) return;
    if (!requestEmptyGraveyard()) {
        logLine("[INFO] The map change could not be asked for; the rush starts in the graveyard as it is.");
        bringInStage();
        return;
    }
    enterPhase(Phase::travelling);
    logLine("[INFO] The player is taken into an empty graveyard.");
}

// The first stage comes once the graveyard has loaded and the player can move.
void updateTravelling() {
    if (inPhaseMs() > travelTimeoutMs) {
        endRush("[ERROR] The empty graveyard was not entered in 30 seconds; the rush stops.", false);
        return;
    }
    const bool ready = emptyGraveyardEntered() && game::currentArea() == static_cast<int32_t>(game::graveyardArea) &&
        game::freeToSpawn() && game::playerParam();
    if (!ready) return;
    const uint8_t* player = game::playerParam();
    rush.playerGroupBefore = game::battleGroup(player);
    logLine("[INFO] The empty graveyard is ready; the first stage waits in it.");
    bringInStage();
}

void announceStage() {
    const RushStage& stage = rush.roster.stages[rush.stage];
    wchar_t name[33];
    const wchar_t* text = nullptr;
    if (stage.title.japanese) {
        text = inGameLanguage(stage.title);
    } else if (game::actorName(rush.fighters[0].param, name)) {
        text = name;
    }
    if (!text) return;
    if (!game::notify(text)) logLine("[WARN] The stage's notification could not be shown.");
}

void settleSheath(Fighter& fighter) {
    if (fighter.sheathSettled) return;
    fighter.sheathSettled = game::settleSheath(fighter.actor) != game::SheathCheck::waiting;
}

// Once: the brain starts the AI's battle mode itself while it fights (0x409290), and each order
// rebuilds the AI's parts. The order also drops the walk it came in with.
void charge(Fighter& fighter) {
    fighter.groupBefore = game::battleGroup(fighter.param);
    if (!game::orderToFight(fighter.actor)) logLine("[WARN] A boss's AI was not found; it may not fight.");
    game::sendAgainst(fighter.actor, fighter.param, game::playerParam(), rush.fighterCount == 1);
}

void updateLoading() {
    bool allReady = true;
    for (uint32_t i = 0; i < rush.fighterCount; ++i) {
        Fighter& fighter = rush.fighters[i];
        if (fighter.param) continue;
        uint8_t* param = game::readyActor(fighter.actor);
        if (!param) {
            allReady = false;
            continue;
        }
        if (!game::placeActor(fighter.actor, fighter.position)) {
            endRush("[ERROR] A boss could not be placed; the rush stops.", true);
            return;
        }
        if (!game::makeRushBoss(fighter.actor, param)) {
            endRush("[ERROR] A boss's action script could not be changed; the rush stops.", true);
            return;
        }
        fighter.param = param;
        charge(fighter);
    }
    if (allReady) {
        announceStage();
        enterPhase(Phase::fighting);
        rush.lastReachedAt = GetTickCount64();
        return;
    }
    if (inPhaseMs() > loadingTimeoutMs) endRush("[ERROR] The bosses did not finish loading in 20 seconds; the rush stops.", true);
}

uint32_t standingFighters() {
    uint32_t standing = 0;
    for (uint32_t i = 0; i < rush.fighterCount; ++i) standing += rush.fighters[i].down ? 0 : 1;
    return standing;
}

void giveUpUnreachedStage() {
    game::notify(inGameLanguage(bossesGone));
    endRush("[WARN] No boss came within 30 units of the player for 60 seconds; the bosses were removed and the exits opened.",
        true);
}

void updateFighting() {
    uint8_t* player = game::playerParam();
    // The game gives the attack only to the fighter the player targets (0x440270): the last one
    // standing is told to attack anyway.
    const bool alone = standingFighters() == 1;
    bool anyStanding = false;
    for (uint32_t i = 0; i < rush.fighterCount; ++i) {
        Fighter& fighter = rush.fighters[i];
        if (fighter.down) continue;
        fighter.param = game::actorParam(fighter.actor);  // The game may move it
        if (!fighter.param) {
            if (!fighter.missingSince) fighter.missingSince = GetTickCount64();
            if (GetTickCount64() - fighter.missingSince < missingParamsLimitMs) {
                anyStanding = true;
                continue;
            }
            fighter.down = true;
            char message[112];
            sprintf_s(message, "[WARN] Actor %d has had no parameters for 3 seconds; it counts as gone.", fighter.actor);
            logLine(message);
            continue;
        }
        fighter.missingSince = 0;
        if (game::defeated(fighter.param)) {
            fighter.down = true;
            char message[64];
            sprintf_s(message, "[INFO] Actor %d was defeated.", fighter.actor);
            logLine(message);
            continue;
        }
        anyStanding = true;
        settleSheath(fighter);
        game::keepFighting(fighter.param, player, alone);
        if (player && withinReach(fighter.param, player)) rush.lastReachedAt = GetTickCount64();
    }
    if (!anyStanding) {
        enterPhase(Phase::resting);
        return;
    }
    if (GetTickCount64() - rush.lastReachedAt >= unreachedLimitMs) giveUpUnreachedStage();
}

void updateResting() {
    if (inPhaseMs() < restBetweenStagesMs) return;
    if (++rush.stage < rush.roster.stageCount) {
        bringInStage();
        return;
    }
    game::notify(inGameLanguage(allDefeated));
    char message[96];
    sprintf_s(message, "[INFO] The player defeated every stage of the %s.", groupName(rush.group));
    endRush(message, false, true);
}

}  // namespace

bool startRush(RushGroup group, LogFunction logFunction) {
    log = logFunction;
    if (rush.phase != Phase::idle) {
        logLine("[WARN] A rush is already running; the new one does not start.");
        return false;
    }
    if (game::currentArea() != static_cast<int32_t>(game::graveyardArea) || !game::freeToSpawn()) {
        logLine("[WARN] The rush starts only in the graveyard outside events; nothing starts.");
        return false;
    }
    rush.group = group;
    rush.roster = rosterOf(group);
    rush.stage = 0;
    rush.area = game::currentArea();
    const uint8_t* player = game::playerParam();
    rush.playerGroupBefore = player ? game::battleGroup(player) : -1;
    game::setInflowStopped(true);
    setExitsLocked(true);
    if (travelAvailable()) {
        game::notify(inGameLanguage(travelNotice));
        enterPhase(Phase::announcing);
        return true;
    }
    logLine("[INFO] The rush starts in the graveyard as it is.");
    return bringInStage();
}

void updateRush() {
    if (rush.phase == Phase::idle) {
        keepOffFallenPlayer();
        updateGraveyardReturn();
        return;
    }
    if (game::currentArea() != rush.area) {
        endRush("[INFO] The player left the area; the rush ends.", false);
        return;
    }
    const uint8_t* player = game::playerParam();
    // From the first frame without HP: the bosses stop being set on the player while the game
    // takes them out of the fight.
    if (player && game::fallen(player)) {
        for (uint32_t i = 0; i < rush.fighterCount; ++i) {
            Fighter& fighter = rush.fighters[i];
            if (!fighter.param || fighter.down) continue;
            fighter.param = game::actorParam(fighter.actor);  // The game may move it
            if (!fighter.param) continue;
            game::leaveFight(fighter.param);
            game::setBattleGroup(fighter.param, fighter.groupBefore);
        }
        // endRush forgets the fighters but leaves them in the array, which keepOffFallenPlayer reads.
        rush.fallenWatchCount = rush.fighterCount;
        rush.fallenWatchUntil = GetTickCount64() + fallenPlayerWatchMs;
        endRush("[INFO] The player fell; the rush ends and the game goes on as usual.", false);
        return;
    }
    switch (rush.phase) {
    case Phase::announcing: updateAnnouncing(); break;
    case Phase::travelling: updateTravelling(); break;
    case Phase::clearing: updateClearing(); break;
    case Phase::loading: updateLoading(); break;
    case Phase::fighting: updateFighting(); break;
    case Phase::resting: updateResting(); break;
    case Phase::idle: break;
    }
}

bool rushRunning() {
    return rush.phase != Phase::idle;
}

}  // namespace boss_rush
