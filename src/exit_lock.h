#pragma once
// Keeps the player from walking out of the area while a rush runs.
#include <wots4/mod_api.h>

namespace boss_rush {

// Checks the code, reserves it and hooks it. False (and logged) if this exe differs or another
// Mod already hooks it; the rush then runs with the exits open.
bool installExitLock(const wots4::ModContext& context, void (*log)(const char* message));
void setExitsLocked(bool locked);

}  // namespace boss_rush
