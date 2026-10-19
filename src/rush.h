#pragma once
// The fights of one rush: each stage's bosses come in, fight the player, and the next stage
// follows 3 seconds after the last of them falls. Driven by onFrame on the game thread.
#include "rush_roster.h"

namespace boss_rush {

using LogFunction = void (*)(const char* message);

// Starts a rush in the current area. False (and logged) if one runs or it cannot start here.
bool startRush(RushGroup group, LogFunction log);
// Once per frame.
void updateRush();
bool rushRunning();

}  // namespace boss_rush
