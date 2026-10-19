#pragma once
// Takes the player into an empty graveyard: a map change like fast travel's, without the time
// going on, into a graveyard where nobody is placed and nobody walks in.
#include <wots4/mod_api.h>

namespace boss_rush {

// Checks the code, reserves it and hooks it. False (and logged) if this exe differs or another
// Mod hooks it; the rush then starts where the player stands. On false, the reservations it made
// may remain: the caller releases them (releaseClaims also drops the Mod's other ones).
bool installTravel(const wots4::ModContext& context, void (*log)(const char* message));
bool travelAvailable();

// Asks the game to load the graveyard again. False if the game state is not there.
bool requestEmptyGraveyard();
// True once the requested graveyard was entered empty (its map is still loading then).
bool emptyGraveyardEntered();
// Lets the graveyard fill as usual again from the next entry, and the townspeople walk in now.
// won: the player beat every stage; walking out of the empty graveyard then leads into the usual one.
void endEmptyGraveyard(bool won);
// Each frame without a rush: drops that redirection once the player left the graveyard or chose
// a fast travel destination, which goes where the player chose.
void updateGraveyardReturn();

}  // namespace boss_rush
