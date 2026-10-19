// The map change: the request fast travel also uses (the destination and the change request of
// the game state, which the action's exit handling 0x4e67c0 commits), with two hooks:
// - CommitActionMapTransition (0x4e7210) gets "skip the time going on" for the Mod's request.
// - After the map entry's event candidates are built (0x572d40), which clears the event
//   system's entry flags each time, the requested entry stops the area's placement, the
//   townspeople walking in, and the occupation characters (as an event script's
//   Character.SetDisableAsChara and SetInitializeEvent do).
#include "travel.h"
#include <MinHook.h>
#include <cstring>

namespace boss_rush {
namespace {

constexpr uintptr_t preferredImageBase = 0x400000;
constexpr uintptr_t gameState = 0x9e967c;
constexpr uintptr_t eventSystem = 0x9eb5f8;
constexpr uint32_t stateArea = 0x2b;                 // int8: the area the player is in
constexpr uint32_t stateDestinationArea = 0x48b9a;   // int8
// int8; the world map's chosen area (0x6423b0), -1 once the menu turned it into the change request
// (0x4e1300). Set over the frames the world map takes to close.
constexpr uint32_t stateFastTravelArea = 0x48b9b;
constexpr int8_t noFastTravel = -1;
constexpr uint32_t stateChangeRequested = 0x48b9c;   // byte; the exit handling commits it next
constexpr int8_t graveyardArea = 0x15;
constexpr uint32_t eventsNoPlacement = 0x1324;       // byte; the area places nobody (0x439830)
constexpr uint32_t eventsNoInflow = 0x1325;          // byte; nobody walks in (0x43a5c0)
constexpr uint32_t eventsNoOccupationCharacters = 0x1327;  // byte (0x50d170)

constexpr uintptr_t commitTransition = 0x4e7210;     // stdcall(action, skipTimeGoingOn); ret 8
// push ebp; mov ebp,esp; sub esp,0xc
constexpr uint8_t commitTransitionStart[] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c};
constexpr uintptr_t buildEntryCandidates = 0x572d40;  // esi = event system; ret
// push ecx; push ebx; push edi; push esi; mov word ptr [esi+0x1324],0
constexpr uint8_t buildEntryCandidatesStart[] = {0x51, 0x53, 0x57, 0x56, 0x66, 0xc7, 0x86, 0x24, 0x13, 0x00, 0x00, 0x00, 0x00};

// The guide shown at an exit ("この先 " / "To " and the area's name, 0x4e690a) is built from the
// exit's own destination in edi, not from the destination the commit uses. A cdecl byte argument
// (the guide's form), so the caller pops it.
constexpr uintptr_t showExitGuide = 0x4df400;        // edi = destination area, push form
// push ebp; mov ebp,esp; push -1
constexpr uint8_t showExitGuideStart[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};

uintptr_t gameBase;
bool available;
volatile LONG travelRequested;  // Set by the Mod, taken by the commit
volatile LONG entering;         // From the commit to the entry
volatile LONG entered;          // The empty graveyard was entered
volatile LONG returnOnExit;     // Walking out of the empty graveyard leads into the usual one
int32_t skipThisCommit;
uint8_t occupationCharactersBefore;
void* originalCommitTransition;
void* originalBuildEntryCandidates;
void* originalShowExitGuide;

template <class T>
T global(uintptr_t virtualAddress) {
    return *reinterpret_cast<T*>(gameBase + virtualAddress - preferredImageBase);
}

// [esp+4] the action, [esp+8] skipTimeGoingOn.
// Before the game commits a map change: the Mod's own change goes into the empty graveyard without
// the time going on; after a won rush, leaving the empty graveyard leads into the usual one.
// Returns 1 to skip the time going on.
int32_t __cdecl beforeCommit() {
    if (travelRequested) {
        travelRequested = 0;
        entering = 1;
        return 1;
    }
    if (!returnOnExit) return 0;
    returnOnExit = 0;
    if (uint8_t* state = global<uint8_t*>(gameState)) state[stateDestinationArea] = static_cast<uint8_t>(graveyardArea);
    return 0;
}

// [esp+4] the action, [esp+8] skipTimeGoingOn.
__declspec(naked) void commitTransitionHook() {
    __asm {
        pushfd
        pushad
        call beforeCommit
        mov skipThisCommit, eax
        popad
        popfd
        cmp skipThisCommit, 0
        je gameDecides
        mov dword ptr [esp + 8], 1
    gameDecides:
        jmp dword ptr [originalCommitTransition]
    }
}

void __cdecl afterEntryCandidates(uint8_t* events) {
    if (!entering) return;
    entering = 0;
    events[eventsNoPlacement] = 1;
    events[eventsNoInflow] = 1;
    occupationCharactersBefore = events[eventsNoOccupationCharacters];
    events[eventsNoOccupationCharacters] = 1;
    entered = 1;
}

__declspec(naked) void buildEntryCandidatesHook() {
    __asm {
        call dword ptr [originalBuildEntryCandidates]
        pushfd
        pushad
        push esi
        call afterEntryCandidates
        add esp, 4
        popad
        popfd
        ret
    }
}

// While leaving the empty graveyard leads into the usual one, the guide names the graveyard.
__declspec(naked) void showExitGuideHook() {
    __asm {
        cmp returnOnExit, 0
        je gameDecides
        push edi
        mov edi, 0x15            ; graveyardArea
        push dword ptr [esp + 8] ; the form
        call dword ptr [originalShowExitGuide]
        add esp, 4
        pop edi
        ret
    gameDecides:
        jmp dword ptr [originalShowExitGuide]
    }
}

struct Site {
    uintptr_t address;
    const uint8_t* bytes;
    uint32_t length;
    void* detour;
    void** original;
};

}  // namespace

bool installTravel(const wots4::ModContext& context, void (*log)(const char* message)) {
    gameBase = reinterpret_cast<uintptr_t>(context.game);
    const Site sites[] = {
        {commitTransition, commitTransitionStart, sizeof(commitTransitionStart), commitTransitionHook, &originalCommitTransition},
        {buildEntryCandidates, buildEntryCandidatesStart, sizeof(buildEntryCandidatesStart), buildEntryCandidatesHook,
            &originalBuildEntryCandidates},
        {showExitGuide, showExitGuideStart, sizeof(showExitGuideStart), showExitGuideHook, &originalShowExitGuide},
    };
    for (const Site& site : sites) {
        const uintptr_t target = gameBase + site.address - preferredImageBase;
        if (memcmp(reinterpret_cast<const void*>(target), site.bytes, site.length)) {
            log("[WARN] The map change code is not as expected; the rush starts where the player stands.");
            return false;
        }
        if (!context.host->claim(context.module, target, site.length)) {
            log("[WARN] Another Mod hooks the map change code; the rush starts where the player stands.");
            return false;
        }
    }
    const MH_STATUS initialized = MH_Initialize();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) {
        log("[WARN] MinHook could not start; the rush starts where the player stands.");
        return false;
    }
    size_t created = 0;
    for (; created < sizeof(sites) / sizeof(sites[0]); ++created) {
        void* target = reinterpret_cast<void*>(gameBase + sites[created].address - preferredImageBase);
        if (MH_CreateHook(target, sites[created].detour, sites[created].original) != MH_OK || MH_EnableHook(target) != MH_OK)
            break;
    }
    if (created == sizeof(sites) / sizeof(sites[0])) {
        available = true;
        return true;
    }
    for (size_t i = 0; i <= created && i < sizeof(sites) / sizeof(sites[0]); ++i)
        MH_RemoveHook(reinterpret_cast<void*>(gameBase + sites[i].address - preferredImageBase));
    log("[WARN] The map change code could not be hooked; the rush starts where the player stands.");
    return false;
}

bool travelAvailable() {
    return available;
}

bool requestEmptyGraveyard() {
    uint8_t* state = global<uint8_t*>(gameState);
    if (!available || !state) return false;
    entered = 0;
    entering = 0;
    // A won rush's redirection would otherwise send the rush's own map changes back here.
    returnOnExit = 0;
    travelRequested = 1;
    state[stateDestinationArea] = static_cast<uint8_t>(graveyardArea);
    state[stateChangeRequested] = 1;
    return true;
}

bool emptyGraveyardEntered() {
    return entered != 0;
}

void endEmptyGraveyard(bool won) {
    travelRequested = 0;
    entering = 0;
    if (!entered) return;
    entered = 0;
    returnOnExit = won ? 1 : 0;
    uint8_t* events = global<uint8_t*>(eventSystem);
    if (!events) return;
    events[eventsNoInflow] = 0;
    events[eventsNoOccupationCharacters] = occupationCharactersBefore;
}

void updateGraveyardReturn() {
    if (!returnOnExit) return;
    const uint8_t* state = global<const uint8_t*>(gameState);
    if (!state || static_cast<int8_t>(state[stateArea]) != graveyardArea ||
        static_cast<int8_t>(state[stateFastTravelArea]) != noFastTravel)
        returnOnExit = 0;
}

}  // namespace boss_rush
