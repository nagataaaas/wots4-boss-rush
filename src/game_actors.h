#pragma once
// The game functions and fields Boss Rush uses to bring in the bosses and to watch them.
// Game thread only. Every call is guarded: a fault returns the failure value instead of
// crashing the game.
#include <windows.h>
#include <cstdint>

namespace boss_rush::game {

constexpr uint32_t graveyardArea = 0x15;
constexpr int32_t noActor = -1;

// Set once by mod.cpp: where the exe is loaded.
void setGameBase(uintptr_t base);

// The area the player is in, or -1 before a game is made.
int32_t currentArea();
// False while an event script or an event battle runs, or before the characters are loaded.
bool freeToSpawn();

enum class SpawnPoint {
    none,          // The player is not loaded
    runIn,         // A point the game's off-screen enemies run in from
    besidePlayer,  // The map has no such point far enough
};
// Where a boss comes in: the map's run-in point nearest the player, at least 20 units away and
// not one of the taken points (the stage's other bosses).
SpawnPoint pickSpawnPoint(float (&position)[3], uint32_t& runInPointCount, const float (*taken)[3], uint32_t takenCount);
// Asks the game to bring in definition number definition (AcNPCDefine), as an event script does.
// Returns its actor ID, or noActor. Place it with placeActor once it is ready.
int32_t queueActor(uint32_t definition, const float (&position)[3]);
// Puts a ready actor at position, as the game places the characters it brings in (0x43a2d4).
bool placeActor(int32_t actor, const float (&position)[3]);
// The actor ID the game gives definition number definition, or noActor if it gives a new one each time.
int32_t actorIdOf(uint32_t definition);
// The actor's CharacterParam once it is loaded and has a brain, else nullptr.
uint8_t* readyActor(int32_t actor);
// The actor's CharacterParam, or nullptr.
uint8_t* actorParam(int32_t actor);
uint8_t* playerParam();
// Makes a loaded actor a boss of the rush: its death changes no story state, and it fights the player.
bool makeRushBoss(int32_t actor, uint8_t* param);
// Makes the actor drop the walk it came in with and fight, as the game's AI command does.
bool orderToFight(int32_t actor);
// Once, when it is placed: turns it toward the player and sets the player as its target.
// alone: the stage's only boss, which attacks without waiting its turn.
void sendAgainst(int32_t actor, uint8_t* boss, uint8_t* player, bool alone);
// Each frame while it lives: keeps it killable and set on the player, adding to the battle flags
// the game keeps.
void keepFighting(uint8_t* boss, uint8_t* player, bool alone);
bool defeated(const uint8_t* param);
// Defeated, or out of HP before the game marks it so (the player as the death begins).
bool fallen(const uint8_t* param);
// The side an actor fights on (see sendAgainst, which puts the player and the bosses on two sides).
int32_t battleGroup(const uint8_t* param);
void setBattleGroup(uint8_t* param, int32_t group);
void removeActor(int32_t actor);

// Diagnostics: where an actor stands, and what its brain does.
struct ActorReport {
    float position[3];
    int32_t brainState;   // The action script's function: 11 fights, 13 died; -1 without a brain
    int32_t brainTarget;  // The actor ID it targets, or -1
    uint32_t flags;       // CharacterParam +0x114
    uint32_t forcedTargetFlags;
    uint32_t removalFlags;  // CharacterParam +0x2f8
    uint32_t roleFlags;     // CharacterParam +0x300
};
bool reportActor(int32_t actor, const uint8_t* param, ActorReport& report);

// Stops (or lets again) townspeople walk into the area.
bool setInflowStopped(bool stopped);

// The actor's name as the game shows it, copied into name (up to 32 characters). False if unknown.
bool actorName(const uint8_t* param, wchar_t (&name)[33]);
// Shows text in the game's notifications (the queue that announces opponents).
bool notify(const wchar_t* text);
// True when the game's texts are Japanese.
bool textsInJapanese();
// The game's state in the layout of a save, or nullptr.
const uint8_t* saveLayout();

}  // namespace boss_rush::game
