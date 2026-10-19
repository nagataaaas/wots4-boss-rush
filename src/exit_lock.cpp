// The game asks GetMapExitBlockReason (0x4e6d70) before it takes the player through an exit of
// the area (0x4e68eb); anything but 0 keeps the exit closed, as during an event battle.
#include "exit_lock.h"
#include <MinHook.h>
#include <cstring>

namespace boss_rush {
namespace {

constexpr uintptr_t preferredImageBase = 0x400000;
constexpr uintptr_t exitBlockReason = 0x4e6d70;  // edi = destination, push current area; ret 4; eax = reason
// push ebp; mov ebp,esp; xor eax,eax: whole instructions for the hook's jump
constexpr uint8_t exitBlockReasonStart[] = {0x55, 0x8b, 0xec, 0x33, 0xc0};
// Reason 3 closes walking exits of every kind (0, 1 and 7) without the other cases' handling
// of reasons 5 and 7. Written as an immediate in the hook below.
[[maybe_unused]] constexpr uint32_t blockedReason = 3;

volatile LONG exitsLocked;
void* originalExitBlockReason;
uintptr_t hookedTarget;  // 0 until hooked

__declspec(naked) void exitBlockReasonHook() {
    __asm {
        cmp exitsLocked, 0
        je gameDecides
        mov eax, 3             ; blockedReason
        ret 4
    gameDecides:
        jmp dword ptr [originalExitBlockReason]
    }
}

}  // namespace

bool installExitLock(const wots4::ModContext& context, void (*log)(const char* message)) {
    const uintptr_t target = reinterpret_cast<uintptr_t>(context.game) + exitBlockReason - preferredImageBase;
    if (memcmp(reinterpret_cast<const void*>(target), exitBlockReasonStart, sizeof(exitBlockReasonStart))) {
        log("[WARN] The area exit check is not as expected; the exits stay open during a rush.");
        return false;
    }
    if (!context.host->claim(context.module, target, sizeof(exitBlockReasonStart))) {
        log("[WARN] Another Mod hooks the area exit check; the exits stay open during a rush.");
        return false;
    }
    const MH_STATUS initialized = MH_Initialize();
    void* function = reinterpret_cast<void*>(target);
    if ((initialized == MH_OK || initialized == MH_ERROR_ALREADY_INITIALIZED) &&
        MH_CreateHook(function, reinterpret_cast<void*>(exitBlockReasonHook), &originalExitBlockReason) == MH_OK &&
        MH_EnableHook(function) == MH_OK) {
        hookedTarget = target;
        return true;
    }
    MH_RemoveHook(function);
    context.host->releaseClaims(context.module);
    log("[WARN] The area exit check could not be hooked; the exits stay open during a rush.");
    return false;
}

void reclaimExitLock(const wots4::ModContext& context) {
    if (hookedTarget) context.host->claim(context.module, hookedTarget, sizeof(exitBlockReasonStart));
}

void setExitsLocked(bool locked) {
    InterlockedExchange(&exitsLocked, locked ? 1 : 0);
}

}  // namespace boss_rush
