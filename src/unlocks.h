#pragma once
// What the player has cleared, read from the game's state in the layout of a save.
#include <cstdint>
#include <cstring>

namespace boss_rush {

// Save offsets. The game keeps a save's bytes at [0x9e967c] + offset + 0x18 (it copies that
// range whole when it writes and reads a save).
constexpr uint32_t eventRecords = 0x3838;      // u8 per event, kept across playthroughs; 4 once it succeeded
constexpr uint32_t eventStates = 0x36a8;       // u8 per event, this playthrough; 4 once it succeeded
constexpr uint32_t occupationClears = 0x33a4;  // u16 per occupation: times cleared across playthroughs
constexpr uint32_t lightOfHopeEvent = 90;      // The ending of the true route
constexpr uint8_t eventSucceeded = 4;
constexpr uint16_t occupationNotOffered = 0xffff;
constexpr uint32_t shitennoVsYoningumiOccupation = 207;  // 四天王 対 四人組
constexpr uint32_t amihamaEightOccupation = 208;         // あみはま八人衆

struct Unlocks {
    bool lightOfHope = false;
    bool shitennoVsYoningumi = false;
    bool amihamaEight = false;
};

inline bool occupationCleared(const uint8_t* save, uint32_t occupation) {
    uint16_t clears;
    memcpy(&clears, save + occupationClears + occupation * sizeof(uint16_t), sizeof(clears));
    return clears != occupationNotOffered && clears > 0;
}

// save points to offset 0 of the save's layout.
inline Unlocks readUnlocks(const uint8_t* save) {
    Unlocks unlocks;
    // The ending ends the playthrough, which copies it to the records; this playthrough's
    // state is read too in case the records are written later.
    unlocks.lightOfHope = save[eventRecords + lightOfHopeEvent] == eventSucceeded ||
        save[eventStates + lightOfHopeEvent] == eventSucceeded;
    unlocks.shitennoVsYoningumi = occupationCleared(save, shitennoVsYoningumiOccupation);
    unlocks.amihamaEight = occupationCleared(save, amihamaEightOccupation);
    return unlocks;
}

}  // namespace boss_rush
