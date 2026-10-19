#pragma once
// The game functions and fields Boss Rush uses to bring in the bosses and to watch them.
// Game thread only. Calls into the game and walks through its objects are guarded: a fault
// returns the failure value instead of crashing the game. The accessors of a CharacterParam
// (battleGroup, setBattleGroup, sendAgainst, keepFighting, defeated, fallen, position) are not, as they run
// every frame for each boss: pass them a pointer looked up this frame (actorParam, readyActor, playerParam).
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

// The map's entrances at least 20 units from the player, nearest first, up to capacity.
uint32_t listEntrances(float (*entrances)[3], uint32_t capacity);
// Where a boss comes in: a run-in point (where the game's off-screen enemies run in from) 50 to
// 110 units from the player, behind the camera and nearest side (an entrance, or nullptr for
// nearest the player), not one of the taken points (the stage's other bosses). Without one, the
// run-in point nearest the player at least 20 units away, or beside the player. False if the
// player is not loaded.
bool pickSpawnPoint(const float* side, float (&position)[3], const float (*taken)[3], uint32_t takenCount);
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
enum class SheathCheck {
    waiting,  // Its sword's model is not built yet
    fine,     // Nothing to do
    moved,    // It had drawn before the build; the sheath went from the sword to the waist
};
// Each frame until it is not waiting: once the actor's sword model is built, a sword drawn
// before that has its sheath put on the waist.
SheathCheck settleSheath(int32_t actor);
// Makes the actor drop the walk it came in with and fight, as the game's AI command does.
bool orderToFight(int32_t actor);
// Once, when it is placed: sets the player as its target.
// alone: the only boss standing. The game gives the attack to the fighter the player targets and
// keeps the others waiting (0x440270), so a lone boss would wait while the player targets nobody.
void sendAgainst(int32_t actor, uint8_t* boss, uint8_t* player, bool alone);
// Each frame while it lives: keeps it killable and set on the player, adding to the battle flags
// the game keeps.
void keepFighting(uint8_t* boss, uint8_t* player, bool alone);
// Each frame after the player fell: no target and no battle flags, as the game's own way out of
// a fight ends (0x40a210). With the flags clear, the brain takes the boss out step by step: the
// AI's battle mode goes, the sword is sheathed with its motion, then it leaves. Battle flag 0x1
// otherwise keeps it among the fighters (0x43fbf0), and the brain out of the fight never sheathes it (AdvanceBrainState 0x409290).
void leaveFight(uint8_t* boss);
bool defeated(const uint8_t* param);
// Defeated, or out of HP before the game marks it so (the player as the death begins).
bool fallen(const uint8_t* param);
// The side an actor fights on (see sendAgainst, which puts the player and the bosses on two sides).
int32_t battleGroup(const uint8_t* param);
void setBattleGroup(uint8_t* param, int32_t group);
// Where the actor stands: float[3].
const float* position(const uint8_t* param);
void removeActor(int32_t actor);

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
