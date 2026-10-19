#include "rush.h"
#include "exit_lock.h"
#include "game_actors.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

namespace boss_rush {
namespace {

constexpr ULONGLONG restBetweenStagesMs = 3000;
constexpr ULONGLONG loadingTimeoutMs = 20000;  // Longer than any model load seen in the game
// A corpse of a boss stays for the player to search, until the same boss comes in again; the
// game brings in no second actor with the ID of one that is there (0x43a760).
constexpr ULONGLONG corpseRemovalMs = 300;
// While the game brings an actor in, its parameters may be missing for a moment.
constexpr ULONGLONG missingParamsLimitMs = 3000;
// Prototype diagnostics: each boss's place and brain, once a second for the first seconds of a stage.
constexpr ULONGLONG reportIntervalMs = 1000;
constexpr uint32_t reportsPerStage = 15;

constexpr wots4::LocalizedText allDefeated{L"強者たちをすべて倒した", L"You have defeated all the strong."};

// Story state a boss's death may write, compared after each stage: the townspeople's records
// (save 0x15c, 12 bytes each), the occupations' flags, and the main characters and factions.
struct WatchedRange {
    uint32_t offset;
    uint32_t size;
};
constexpr WatchedRange watchedRanges[] = {{0x0100, 0x40}, {0x015c, 12 * 128}, {0x3740, 0x50}};
constexpr uint32_t maxLoggedChanges = 16;

enum class Phase { idle, clearing, loading, fighting, resting };

struct Fighter {
    int32_t actor;
    uint8_t* param;  // Once loaded
    bool down;
    float position[3];  // Where it comes in
    ULONGLONG missingSince;  // When its parameters went missing, or 0
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
    ULONGLONG lastReport = 0;
    uint32_t reportCount = 0;
    uint8_t watched[0x40 + 12 * 128 + 0x50]{};
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

void copyWatched(uint8_t* target) {
    const uint8_t* save = game::saveLayout();
    if (!save) return;
    for (const WatchedRange& range : watchedRanges) {
        memcpy(target, save + range.offset, range.size);
        target += range.size;
    }
}

// Logs the story bytes that changed since the rush started. Diagnostics of the prototype.
void logStoryChanges() {
    uint8_t now[sizeof(rush.watched)];
    copyWatched(now);
    uint32_t index = 0, logged = 0, changed = 0;
    for (const WatchedRange& range : watchedRanges) {
        for (uint32_t i = 0; i < range.size; ++i, ++index) {
            if (now[index] == rush.watched[index]) continue;
            ++changed;
            if (logged == maxLoggedChanges) continue;
            ++logged;
            char message[128];
            sprintf_s(message, "[WARN] Story state changed during the rush: save offset 0x%04x from 0x%02x to 0x%02x.",
                range.offset + i, rush.watched[index], now[index]);
            logLine(message);
        }
    }
    char message[128];
    sprintf_s(message, "[INFO] Story state check after stage %u: %u byte(s) changed.", rush.stage + 1, changed);
    logLine(message);
}

void reportFighters(const char* when) {
    const uint8_t* player = game::playerParam();
    game::ActorReport playerReport{};
    if (!player || !game::reportActor(0, player, playerReport)) return;
    for (uint32_t i = 0; i < rush.fighterCount; ++i) {
        const Fighter& fighter = rush.fighters[i];
        const uint8_t* param = fighter.param ? fighter.param : game::actorParam(fighter.actor);
        game::ActorReport report{};
        if (!param || !game::reportActor(fighter.actor, param, report)) {
            char message[96];
            sprintf_s(message, "[DEBUG] %s: actor %d has no parameters yet.", when, fighter.actor);
            logLine(message);
            continue;
        }
        const float dx = report.position[0] - playerReport.position[0];
        const float dy = report.position[1] - playerReport.position[1];
        const float dz = report.position[2] - playerReport.position[2];
        char message[320];
        sprintf_s(message,
            "[DEBUG] %s: actor %d at (%.1f, %.1f, %.1f), player at (%.1f, %.1f, %.1f), distance %.1f; "
            "brain state %d, target %d; flags 0x%08x, battle flags 0x%08x, removal 0x%08x, roles 0x%08x.",
            when, fighter.actor, report.position[0], report.position[1], report.position[2], playerReport.position[0],
            playerReport.position[1], playerReport.position[2], sqrtf(dx * dx + dy * dy + dz * dz), report.brainState,
            report.brainTarget, report.flags, report.forcedTargetFlags, report.removalFlags, report.roleFlags);
        logLine(message);
    }
}

void reportFightersEverySecond() {
    if (rush.reportCount >= reportsPerStage || GetTickCount64() - rush.lastReport < reportIntervalMs) return;
    rush.lastReport = GetTickCount64();
    ++rush.reportCount;
    reportFighters("fighting");
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

void endRush(const char* reason, bool removeThem) {
    if (removeThem) removeFighters();
    if (uint8_t* player = game::playerParam()) game::setBattleGroup(player, rush.playerGroupBefore);
    setExitsLocked(false);
    rush.fighterCount = 0;
    game::setInflowStopped(false);
    enterPhase(Phase::idle);
    logLine(reason);
}

bool queueStage() {
    const RushStage& stage = rush.roster.stages[rush.stage];
    rush.fighterCount = 0;
    for (uint32_t i = 0; i < stage.fighterCount; ++i) {
        float position[3];
        uint32_t runInPoints = 0;
        float taken[maxStageFighters][3];
        for (uint32_t j = 0; j < rush.fighterCount; ++j) memcpy(taken[j], rush.fighters[j].position, sizeof(taken[j]));
        const game::SpawnPoint point = game::pickSpawnPoint(position, runInPoints, taken, rush.fighterCount);
        const int32_t actor =
            point == game::SpawnPoint::none ? game::noActor : game::queueActor(stage.definitions[i], position);
        if (point != game::SpawnPoint::none) {
            char where[192];
            sprintf_s(where, "[DEBUG] Definition %u comes in at (%.1f, %.1f, %.1f): %s (the map has %u run-in point(s)).",
                stage.definitions[i], position[0], position[1], position[2],
                point == game::SpawnPoint::runIn ? "a run-in point" : "beside the player", runInPoints);
            logLine(where);
        }
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

void announceStage() {
    const RushStage& stage = rush.roster.stages[rush.stage];
    std::wstring text;
    if (stage.title.japanese) {
        text = inGameLanguage(stage.title);
    } else {
        wchar_t name[33];
        if (!game::actorName(rush.fighters[0].param, name)) return;
        text = name;
    }
    if (!game::notify(text.c_str())) logLine("[WARN] The stage's notification could not be shown.");
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
        // Once: the brain starts the AI's battle mode itself while it fights (0x409290), and each
        // order rebuilds the AI's parts.
        if (!game::orderToFight(fighter.actor)) logLine("[WARN] A boss's AI was not found; it may not fight.");
        game::sendAgainst(fighter.actor, param, game::playerParam(), rush.fighterCount == 1);
        fighter.param = param;
    }
    if (allReady) {
        reportFighters("loaded");
        rush.reportCount = 0;
        rush.lastReport = 0;
        announceStage();
        enterPhase(Phase::fighting);
        return;
    }
    if (inPhaseMs() > loadingTimeoutMs) endRush("[ERROR] The bosses did not finish loading in 20 seconds; the rush stops.", true);
}

void updateFighting() {
    uint8_t* player = game::playerParam();
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
        game::keepFighting(fighter.param, player, rush.fighterCount == 1);
        anyStanding = true;
    }
    if (anyStanding) {
        reportFightersEverySecond();
        return;
    }
    logStoryChanges();
    enterPhase(Phase::resting);
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
    endRush(message, false);
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
    copyWatched(rush.watched);
    game::setInflowStopped(true);
    setExitsLocked(true);
    return bringInStage();
}

void updateRush() {
    if (rush.phase == Phase::idle) return;
    if (game::currentArea() != rush.area) {
        endRush("[INFO] The player left the area; the rush ends.", false);
        return;
    }
    const uint8_t* player = game::playerParam();
    // From the first frame without HP: the bosses stop being set on the player while the game
    // takes them out of the fight.
    if (player && game::fallen(player)) {
        endRush("[INFO] The player fell; the rush ends and the game goes on as usual.", false);
        return;
    }
    switch (rush.phase) {
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
