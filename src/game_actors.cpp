// Addresses are virtual addresses of the supported Steam exe (ImageBase 0x400000). The
// functions take their arguments in registers; each wrapper says how.
#include "game_actors.h"
#include <cmath>
#include <cstring>

namespace boss_rush::game {
namespace {

constexpr uintptr_t preferredImageBase = 0x400000;

// Globals
constexpr uintptr_t gameState = 0x9e967c;           // cpGParamMng*: the save's data
constexpr uintptr_t appendCharacters = 0x9e9640;    // The manager of the characters brought into an area
constexpr uintptr_t characterManager = 0x9e9658;    // Every actor by ID
constexpr uintptr_t actionScripts = 0x9e95d0;       // The brains by actor ID
constexpr uintptr_t mapManager = 0x9e96bc;          // +4: the loaded map, whose +4 is its ID
constexpr uintptr_t eventSystem = 0x9eb5f8;
constexpr uintptr_t eventBattle = 0x9eb5e4;
constexpr uintptr_t notifications = 0x9ebb8c;       // +0x30: the notification queue
constexpr uintptr_t textManager = 0x9e9680;

// Functions
constexpr uintptr_t queueAppendCharacter = 0x43a990;  // ecx = definition, ebx = &position, push context, flags, manager; ret 0xc
constexpr uintptr_t placeAppendCharacter = 0x43a900;  // eax = ID, push &position, manager; ret 8
constexpr uintptr_t findCharacterEntry = 0x465590;    // ecx = character manager, esi = ID; eax = entry
// The points the game's off-screen enemies (WakiZako) run in from, as 0x435fb6 reads them.
constexpr uintptr_t findRunInPoints = 0x43ca10;       // ecx = point data, bx = map ID; eax = the points
constexpr uintptr_t countRunInPoints = 0x43ca80;      // ecx = point data, di = map ID; eax = how many
constexpr uintptr_t changeActionScript = 0x54b060;    // ecx = ID, eax = script
constexpr uintptr_t removeAppendCharacter = 0x439c90; // ecx = ID, push manager; ret 4
// What the AI command that sets a forced target (0x418e40) does first, so that the actor drops
// the walk it was given when it came in and fights:
constexpr uintptr_t findActorAi = 0x41fe30;           // cdecl(ID, kind 1); eax = the AI, or null
constexpr uintptr_t resetAiMovement = 0x428ce0;       // esi = the movement to start again
constexpr uintptr_t startAiBattle = 0x41fd20;         // esi = the AI
// Script op 174 (0x4128e0) pins a character in place: a mode at the entry's +0x20 object, and the
// physics body's flag 0x80 raised through this (0 lowers it). The commander pins itself outside
// its own fight.
constexpr uintptr_t setBodyPinned = 0x526dd0;         // edi = physics, ebx = body, push 0/1; ret 4
constexpr uintptr_t physicsManager = 0x9ecf60;
constexpr uint32_t entryMotion = 0x20;
constexpr uint32_t motionPinnedMode = 0xac;           // 0: free
constexpr uint32_t motionBody = 0x134;                // +0x84: the body's number, below 0x40
constexpr uintptr_t makeGameString = 0x407a90;        // ecx = UTF-16 text, push string storage; ret 4
constexpr uintptr_t queueNotification = 0x5c3ea0;     // esi = queue, bl = 0, push duration, string; ret 8
constexpr uintptr_t freeGameString = 0x407ff0;        // ecx = string storage

// Fields
constexpr uint32_t stateArea = 0x2b;                 // int8: the area the player is in
constexpr uint32_t eventScriptRunning = 0x170;       // byte; 1 while an event script runs
constexpr uint32_t inflowStopped = 0x1325;           // byte; townspeople stop walking into the area
constexpr uint32_t eventBattleState = 0x4e4;         // 0 when no event battle runs
// AcNPCDefine, as 0x43a990 reads it: [[manager]+0x24] -> {count, ..., offset of each row from +0xc}.
constexpr uint32_t managerDefinitions = 0x24;
constexpr uint32_t definitionRowOffsets = 0x0c;
constexpr uint32_t definitionActorId = 0x06;        // int16; -1: a new ID each time (0x43a9d8)
constexpr uint32_t entryParam = 0x0c;                // CharacterParam*
constexpr uint32_t entryLoadState = 0x10;            // 3 once the model is loaded
constexpr uint32_t entryLoaded = 3;
constexpr float spawnHeightOffset = 0.1f;            // As the game adds when it brings in an ambush (0x43a70b)
constexpr uint32_t appendSpawner = 0x08;             // The WakiZako spawner (0x43a727)
constexpr uint32_t spawnerPointData = 0x0c;          // Passed to findRunInPoints (0x435fb3)
constexpr uint32_t runInPointSize = 36;
constexpr uint32_t runInPointPosition = 0x04;        // float[3]
// The map's entrances lie outside the walkable ground, where a fighting brain cannot find its
// way in; the run-in points are inside it. The nearest one at this distance or more.
constexpr float minimumRunInDistance = 20.0f;
constexpr float fallbackOffset = 10.0f;              // Without points: beside the player, on walkable ground
constexpr uint32_t scriptsBrainsHeader = 0x0c;       // std::map<int, Brain*> by actor ID: header, then root
constexpr uint32_t scriptsBrainsRoot = 0x10;
constexpr uint32_t paramFlags = 0x114;               // 0x600000: dead
constexpr uint32_t deadFlags = 0x600000;
// Battle flags. The game's target manager (0x43f520) rewrites the others every frame (chase by
// path 0x100000, turn toward the target 0x100, ...), so the Mod only ever adds these two.
constexpr uint32_t paramBattleFlags = 0x2ec;
constexpr uint32_t battleActive = 0x1;               // As 0x4370d1
constexpr uint32_t battleAlwaysEngage = 0x200000;    // Attack instead of waiting; cleared when the target changes (0x418ec6)
constexpr uint32_t paramForcedTarget = 0x2f0;        // CharacterParam*
constexpr uint32_t paramDeathKeepsRecords = 0x300;   // bit 0: the death does not mark the townsperson dead (0x409756)
// Marks a boss's own script leaves when it starts (function 0), before it becomes BattleMob,
// which never sets them again:
constexpr uint32_t paramRemovalFlags = 0x2f8;
constexpr uint32_t removalRequested = 0x10;          // Script op 181 (0x413450): the manager removes it (0x43a4bf);
                                                     // the boss and Kumagami ask it outside their own scenes
constexpr uint32_t playerAlly = 0x20000;             // In +0x300; script op 175 (0x4133a0): on the player's side,
                                                     // never a lock-on target (the Shitenno set it)
constexpr uint32_t paramSideFlags = 0x2fc;
constexpr uint32_t followsPlayerSide = 0x1000;       // Also the player's side for 0x4d7340
constexpr uint32_t holdsPosition = 0x40000;          // Script op 188 (0x413490); the commander sets it outside its
                                                     // own fight and clears it in it, where it comes at the player
constexpr uint32_t paramHp = 0x1f8;                  // float; the game's own checks count 0 as beaten
constexpr uint32_t paramMinimumHp = 0x328;           // The cut stops at this; the main characters have 1
constexpr uint32_t paramName = 0x2e8;
constexpr uint32_t paramPosition = 0xd4;             // float[3], as the game's distance (0x41c960) reads it
constexpr uint32_t paramOrientation = 0xa4;          // float[3][3]; a new actor faces +z
// The side in a fight: two actors whose groups are both set are allies only in the same group;
// otherwise the same character type makes allies (0x4d7340). It keeps the bosses of a stage on
// one side and the player on the other.
constexpr uint32_t paramBattleGroup = 0x2e0;
constexpr int32_t playerGroup = 0;
constexpr int32_t bossGroup = 1;
constexpr uint32_t aiMovements = 0x18;               // The AI's movements: current, next, ..., default
constexpr uint32_t movementsCurrent = 0x00;
constexpr uint32_t movementsNext = 0x04;
constexpr uint32_t movementsDefault = 0x10;
constexpr uint32_t brainState = 0x24;               // The function it runs (0x4097e6)
constexpr uint32_t brainTarget = 0x44;                // const wchar_t*
constexpr uint32_t nameChars = 32;
constexpr uint32_t textLanguage = 0x4c;              // 0: Japanese
// Flags of queueAppendCharacter: none, as an event script brings in a character (0x549df0). With
// 0x20000 or 0x40000 the brain comes in as a townsperson passing through (0x43a234), and walks
// its way however it is ordered. Written as an immediate in the call below.
[[maybe_unused]] constexpr uint32_t noAppendFlags = 0;
[[maybe_unused]] constexpr int32_t noContext = -1;
// BattleMob: its death function only drops its belongings, unlike the bosses' own scripts,
// which write their story flags when they die.
[[maybe_unused]] constexpr int32_t battleMobScript = 70;
constexpr int32_t playerActor = 0;
[[maybe_unused]] constexpr int32_t notificationDuration = -1;  // The queue's default
constexpr size_t gameStringSize = 0x28;
constexpr uint32_t saveLayoutInState = 0x18;

uintptr_t gameBase;

uintptr_t address(uintptr_t virtualAddress) {
    return gameBase + virtualAddress - preferredImageBase;
}

template <class T>
T global(uintptr_t virtualAddress) {
    return *reinterpret_cast<T*>(address(virtualAddress));
}

template <class T>
T& at(void* object, uint32_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
}

uint8_t* characterEntry(int32_t actor) {
    void* manager = global<void*>(characterManager);
    if (!manager) return nullptr;
    const uintptr_t find = address(findCharacterEntry);
    uint8_t* entry = nullptr;
    __try {
        __asm {
            push esi
            mov ecx, manager
            mov esi, actor
            call find
            pop esi
            mov entry, eax
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return entry;
}

// The brain of an actor, found like 0x54b098 does.
void* brainOf(int32_t actor) {
    uint8_t* scripts = global<uint8_t*>(actionScripts);
    if (!scripts) return nullptr;
    uint8_t* header = scripts + scriptsBrainsHeader;
    uint8_t* node = at<uint8_t*>(scripts, scriptsBrainsRoot);
    uint8_t* candidate = header;
    while (node) {
        if (at<int32_t>(node, 0x10) >= actor) {
            candidate = node;
            node = at<uint8_t*>(node, 0x08);
        } else {
            node = at<uint8_t*>(node, 0x0c);
        }
    }
    if (candidate == header || at<int32_t>(candidate, 0x10) != actor) return nullptr;
    return at<void*>(candidate, 0x14);
}

int32_t loadedMapId() {
    uint8_t* manager = global<uint8_t*>(mapManager);
    uint8_t* map = manager ? at<uint8_t*>(manager, 4) : nullptr;
    return map ? at<int32_t>(map, 4) & 0xffff : -1;
}

float distanceSquared(const float* a, const float* b) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

// The run-in points of the loaded map, or nullptr.
const uint8_t* runInPoints(uint32_t& count) {
    count = 0;
    uint8_t* manager = global<uint8_t*>(appendCharacters);
    uint8_t* spawner = manager ? at<uint8_t*>(manager, appendSpawner) : nullptr;
    void* data = spawner ? at<void*>(spawner, spawnerPointData) : nullptr;
    const int32_t map = loadedMapId();
    if (!data || map < 0) return nullptr;
    const uintptr_t find = address(findRunInPoints);
    const uintptr_t countPoints = address(countRunInPoints);
    const uint8_t* points = nullptr;
    int32_t found = 0;
    __try {
        __asm {
            push ebx
            push edi
            mov ecx, data
            mov ebx, map
            call find
            mov points, eax
            mov ecx, data
            mov edi, map
            call countPoints
            mov found, eax
            pop edi
            pop ebx
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    if (!points || found <= 0) return nullptr;
    count = static_cast<uint32_t>(found);
    return points;
}

}  // namespace

bool isTaken(const float* point, const float (*taken)[3], uint32_t takenCount) {
    constexpr float samePoint = 1.0f;
    for (uint32_t i = 0; i < takenCount; ++i) {
        if (distanceSquared(point, taken[i]) < samePoint * samePoint) return true;
    }
    return false;
}

SpawnPoint pickSpawnPoint(float (&position)[3], uint32_t& runInPointCount, const float (*taken)[3], uint32_t takenCount) {
    uint8_t* player = actorParam(playerActor);
    runInPointCount = 0;
    if (!player) return SpawnPoint::none;
    float playerPosition[3];
    memcpy(playerPosition, player + paramPosition, sizeof(playerPosition));
    const uint8_t* points = runInPoints(runInPointCount);
    const float* best = nullptr;
    float bestDistance = 0;
    for (uint32_t i = 0; i < runInPointCount; ++i) {
        const auto point = reinterpret_cast<const float*>(points + i * runInPointSize + runInPointPosition);
        const float distance = distanceSquared(point, playerPosition);
        if (distance < minimumRunInDistance * minimumRunInDistance || (best && distance >= bestDistance)) continue;
        if (isTaken(point, taken, takenCount)) continue;
        best = point;
        bestDistance = distance;
    }
    const SpawnPoint kind = best ? SpawnPoint::runIn : SpawnPoint::besidePlayer;
    memcpy(position, best ? best : playerPosition, sizeof(position));
    if (!best) position[0] += fallbackOffset * (takenCount + 1);
    position[1] += spawnHeightOffset;
    return kind;
}

void setGameBase(uintptr_t base) {
    gameBase = base;
}

int32_t currentArea() {
    const uint8_t* state = global<const uint8_t*>(gameState);
    return state ? static_cast<int8_t>(state[stateArea]) : -1;
}

bool freeToSpawn() {
    const uint8_t* events = global<const uint8_t*>(eventSystem);
    const uint8_t* battle = global<const uint8_t*>(eventBattle);
    if (!events || !global<void*>(appendCharacters) || !global<void*>(characterManager)) return false;
    if (events[eventScriptRunning] == 1) return false;
    return !battle || at<uint32_t>(const_cast<uint8_t*>(battle), eventBattleState) == 0;
}

int32_t queueActor(uint32_t definition, const float (&position)[3]) {
    void* manager = global<void*>(appendCharacters);
    if (!manager) return noActor;
    const uintptr_t queue = address(queueAppendCharacter);
    const float* where = position;
    int32_t actor = noActor;
    __try {
        __asm {
            push ebx
            mov ecx, definition
            mov ebx, where
            push -1                ; noContext
            push 0                 ; noAppendFlags
            push manager
            call queue
            pop ebx
            mov actor, eax
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return noActor;
    }
    return actor;
}

int32_t actorIdOf(uint32_t definition) {
    uint8_t* manager = global<uint8_t*>(appendCharacters);
    __try {
        uint8_t* holder = manager ? at<uint8_t*>(manager, 0) : nullptr;
        uint8_t* table = holder ? at<uint8_t*>(at<uint8_t*>(holder, managerDefinitions), 0) : nullptr;
        if (!table || at<int32_t>(table, 0) <= static_cast<int32_t>(definition)) return noActor;
        const int32_t row = at<int32_t>(table, definitionRowOffsets + definition * 4);
        if (row <= 0) return noActor;
        const int16_t actor = at<int16_t>(table, row + definitionActorId);
        return actor < 0 ? noActor : actor;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return noActor;
    }
}

uint8_t* actorParam(int32_t actor) {
    uint8_t* entry = characterEntry(actor);
    return entry ? at<uint8_t*>(entry, entryParam) : nullptr;
}

bool placeActor(int32_t actor, const float (&position)[3]) {
    void* manager = global<void*>(appendCharacters);
    if (!manager) return false;
    const uintptr_t place = address(placeAppendCharacter);
    const float* where = position;
    __try {
        __asm {
            push where
            push manager
            mov eax, actor
            call place
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

uint8_t* readyActor(int32_t actor) {
    uint8_t* entry = characterEntry(actor);
    if (!entry || at<uint32_t>(entry, entryLoadState) != entryLoaded || !brainOf(actor)) return nullptr;
    return at<uint8_t*>(entry, entryParam);
}

uint8_t* playerParam() {
    return actorParam(playerActor);
}

// Like 0x418e7f..0x418ea0: the AI's movements (+0x18) go back to the default one (+0x10), then
// the AI starts fighting.
bool orderToFight(int32_t actor) {
    const uintptr_t find = address(findActorAi);
    const uintptr_t reset = address(resetAiMovement);
    const uintptr_t start = address(startAiBattle);
    uint8_t* ai = nullptr;
    __try {
        __asm {
            push 1
            push actor
            call find
            add esp, 8
            mov ai, eax
        }
        if (!ai) return false;
        uint8_t* movements = at<uint8_t*>(ai, aiMovements);
        if (!movements) return false;
        void* standard = at<void*>(movements, movementsDefault);
        at<void*>(movements, movementsNext) = standard;
        if (at<void*>(movements, movementsCurrent) != standard) {
            __asm {
                push esi
                mov esi, standard
                call reset
                pop esi
            }
        }
        __asm {
            push esi
            mov esi, ai
            call start
            pop esi
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

// A boss of the rush is nobody's ally but its stage's, and stays until it falls.
void clearStoryRoles(uint8_t* param) {
    at<uint32_t>(param, paramRemovalFlags) &= ~removalRequested;
    at<uint32_t>(param, paramDeathKeepsRecords) = (at<uint32_t>(param, paramDeathKeepsRecords) | 1) & ~playerAlly;
    at<uint32_t>(param, paramSideFlags) &= ~(followsPlayerSide | holdsPosition);
}

// As op 174 with mode 0 (0x412991): frees a character its script pinned in place.
void unpin(int32_t actor) {
    uint8_t* entry = characterEntry(actor);
    uint8_t* motion = entry ? at<uint8_t*>(entry, entryMotion) : nullptr;
    if (!motion || at<int32_t>(motion, motionPinnedMode) == 0) return;
    at<int32_t>(motion, motionPinnedMode) = 0;
    void* physics = global<void*>(physicsManager);
    uint8_t* body = at<uint8_t*>(motion, motionBody);
    if (!physics || !body) return;
    const uint32_t bodyNumber = at<uint32_t>(body, 0x84);
    const uintptr_t pin = address(setBodyPinned);
    __try {
        __asm {
            push ebx
            push edi
            mov edi, physics
            mov ebx, bodyNumber
            push 0
            call pin
            pop edi
            pop ebx
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool makeRushBoss(int32_t actor, uint8_t* param) {
    const uintptr_t change = address(changeActionScript);
    __try {
        __asm {
            mov ecx, actor
            mov eax, 70            ; battleMobScript
            call change
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    clearStoryRoles(param);
    unpin(actor);
    return true;
}

// As the off-screen enemies are turned when they come in (0x436f1b..0x436f83): the flat direction
// to the player in the matrix's first and third rows.
void faceToward(uint8_t* boss, const uint8_t* player) {
    const float* from = reinterpret_cast<const float*>(boss + paramPosition);
    const float* to = reinterpret_cast<const float*>(player + paramPosition);
    float dx = to[0] - from[0], dz = to[2] - from[2];
    const float length = sqrtf(dx * dx + dz * dz);
    if (length <= 0) return;
    dx /= length;
    dz /= length;
    float* matrix = reinterpret_cast<float*>(boss + paramOrientation);
    matrix[0] = dz;
    matrix[2] = dx;
    matrix[6] = -dx;
    matrix[8] = dz;
}

// Several bosses take turns at the player as the game's waiting fighters do (0x20); a lone boss
// always attacks.
uint32_t battleFlagsFor(bool alone) {
    return alone ? battleActive | battleAlwaysEngage : battleActive;
}

void setOnPlayer(uint8_t* boss, uint8_t* player, bool alone) {
    if (at<uint8_t*>(boss, paramForcedTarget) != player) at<uint8_t*>(boss, paramForcedTarget) = player;
    at<int32_t>(boss, paramBattleGroup) = bossGroup;
    at<int32_t>(player, paramBattleGroup) = playerGroup;
    at<uint32_t>(boss, paramBattleFlags) |= battleFlagsFor(alone);
}

void sendAgainst(int32_t actor, uint8_t* boss, uint8_t* player, bool alone) {
    if (!player) return;
    faceToward(boss, player);
    at<uint8_t*>(boss, paramForcedTarget) = player;
    at<uint32_t>(boss, paramBattleFlags) = battleFlagsFor(alone);
    at<int32_t>(boss, paramBattleGroup) = bossGroup;
    at<int32_t>(player, paramBattleGroup) = playerGroup;
}

int32_t battleGroup(const uint8_t* param) {
    return at<int32_t>(const_cast<uint8_t*>(param), paramBattleGroup);
}

void setBattleGroup(uint8_t* param, int32_t group) {
    at<int32_t>(param, paramBattleGroup) = group;
}

void keepFighting(uint8_t* boss, uint8_t* player, bool alone) {
    at<int32_t>(boss, paramMinimumHp) = 0;
    clearStoryRoles(boss);
    if (player) setOnPlayer(boss, player, alone);
}

bool fallen(const uint8_t* param) {
    return defeated(param) || at<float>(const_cast<uint8_t*>(param), paramHp) <= 0;
}

bool defeated(const uint8_t* param) {
    return (at<uint32_t>(const_cast<uint8_t*>(param), paramFlags) & deadFlags) != 0;
}

void removeActor(int32_t actor) {
    void* manager = global<void*>(appendCharacters);
    if (!manager) return;
    const uintptr_t remove = address(removeAppendCharacter);
    __try {
        __asm {
            mov ecx, actor
            push manager
            call remove
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool reportActor(int32_t actor, const uint8_t* param, ActorReport& report) {
    __try {
        uint8_t* writable = const_cast<uint8_t*>(param);
        memcpy(report.position, writable + paramPosition, sizeof(report.position));
        report.flags = at<uint32_t>(writable, paramFlags);
        report.forcedTargetFlags = at<uint32_t>(writable, paramBattleFlags);
        report.removalFlags = at<uint32_t>(writable, paramRemovalFlags);
        report.roleFlags = at<uint32_t>(writable, paramDeathKeepsRecords);
        uint8_t* brain = static_cast<uint8_t*>(brainOf(actor));
        report.brainState = brain ? at<int32_t>(brain, brainState) : -1;
        report.brainTarget = brain ? at<int32_t>(brain, brainTarget) : -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

bool setInflowStopped(bool stopped) {
    uint8_t* events = global<uint8_t*>(eventSystem);
    if (!events) return false;
    events[inflowStopped] = stopped ? 1 : 0;
    return true;
}

bool actorName(const uint8_t* param, wchar_t (&name)[33]) {
    __try {
        const wchar_t* source = at<const wchar_t*>(const_cast<uint8_t*>(param), paramName);
        if (!source) return false;
        memcpy(name, source, nameChars * sizeof(wchar_t));
        name[nameChars] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return name[0] != 0;
}

bool notify(const wchar_t* text) {
    uint8_t* service = global<uint8_t*>(notifications);
    void* queue = service ? at<void*>(service, 0x30) : nullptr;
    if (!queue || !at<void*>(queue, 0x0c)) return false;
    uint8_t storage[gameStringSize] = {};
    uint8_t* string = storage;
    const uintptr_t make = address(makeGameString);
    const uintptr_t show = address(queueNotification);
    const uintptr_t release = address(freeGameString);
    bool queued = false;
    __try {
        __asm {
            mov ecx, text
            push string
            call make
        }
        __asm {
            push esi
            push ebx
            mov esi, queue
            xor bl, bl
            push -1                ; notificationDuration
            push string
            call show
            pop ebx
            pop esi
        }
        queued = true;
        __asm {
            mov ecx, string
            call release
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return queued;
}

bool textsInJapanese() {
    const uint8_t* manager = global<const uint8_t*>(textManager);
    return manager && at<uint32_t>(const_cast<uint8_t*>(manager), textLanguage) == 0;
}

const uint8_t* saveLayout() {
    const uint8_t* state = global<const uint8_t*>(gameState);
    return state ? state + saveLayoutInState : nullptr;
}

}  // namespace boss_rush::game
