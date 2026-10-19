// Boss Rush: once the true route's ending was seen, the ghost of the graveyard offers to
// fight the strong of a faction or of the town one after another.
#include "exit_lock.h"
#include "travel.h"
#include "game_actors.h"
#include "rush.h"
#include <cstddef>
#include <cstdio>

namespace {

constexpr DWORD supportedExeTimestamp = 0x683d42eb;  // PE TimeDateStamp of the current Steam exe
// The talk closes over a few frames after the last choice; the rush waits for it.
constexpr ULONGLONG startDelayMs = 500;

// The ghost of the graveyard runs action script 143; its choices are texts 601 to 604.
constexpr int32_t ghostScript = 143;
constexpr int32_t ghostChoiceTexts[] = {601, 602, 603, 604};

const wots4::HostApi* host = nullptr;
HMODULE module = nullptr;

boss_rush::RushMenuItems shownItems;  // The Mod menu in the window, for its selected callback
bool rushRequested = false;
ULONGLONG rushRequestedAt = 0;
boss_rush::RushGroup requestedGroup = boss_rush::RushGroup::joui;

void log(const char* message) {
    if (host && host->log) host->log(module, message);
}

boss_rush::Unlocks currentUnlocks() {
    const uint8_t* save = boss_rush::game::saveLayout();
    return save ? boss_rush::readUnlocks(save) : boss_rush::Unlocks{};
}

wots4::ConversationReply WINAPI menuItemSelected(void*, const wots4::ConversationContext*,
    wots4::ConversationSession session, uint32_t item);

wots4::ConversationReply showMenu(boss_rush::RushMenu menu, wots4::ConversationSession session) {
    shownItems = boss_rush::menuItems(menu, currentUnlocks());
    wots4::LocalizedText labels[boss_rush::maxRushMenuItems];
    for (uint32_t i = 0; i < shownItems.count; ++i) labels[i] = shownItems.items[i].label;
    if (!shownItems.count || !host->showConversationMenu(module, session, labels, shownItems.count, menuItemSelected, nullptr))
        return wots4::conversationEnd;
    return wots4::conversationContinue;
}

BOOL WINAPI fightLineShown(void*, const wots4::ConversationContext*) {
    return !boss_rush::rushRunning() && currentUnlocks().lightOfHope;
}

wots4::ConversationReply WINAPI fightSelected(void*, const wots4::ConversationContext*,
    wots4::ConversationSession session, uint32_t) {
    return showMenu(boss_rush::firstMenu(currentUnlocks()), session);
}

// The fights start from the next frame, once the talk has ended.
void requestRush(boss_rush::RushGroup group) {
    rushRequested = true;
    rushRequestedAt = GetTickCount64();
    requestedGroup = group;
    char message[128];
    sprintf_s(message, "[INFO] The player chose to fight the %s at the ghost of the graveyard.", boss_rush::groupName(group));
    log(message);
}

wots4::ConversationReply WINAPI menuItemSelected(void*, const wots4::ConversationContext*,
    wots4::ConversationSession session, uint32_t item) {
    if (item == wots4::conversationCancelled || item >= shownItems.count) return wots4::conversationEnd;
    const boss_rush::RushMenuItem chosen = shownItems.items[item];
    if (chosen.opensMenu) return showMenu(chosen.menu, session);
    requestRush(chosen.group);
    return wots4::conversationEnd;
}

void WINAPI onFrame(void*) {
    if (rushRequested && GetTickCount64() - rushRequestedAt >= startDelayMs) {
        rushRequested = false;
        boss_rush::startRush(requestedGroup, log);
    }
    boss_rush::updateRush();
}

bool exeSupported(HMODULE game) {
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(game);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(reinterpret_cast<const uint8_t*>(game) + dos->e_lfanew);
    return dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE &&
        nt->FileHeader.TimeDateStamp == supportedExeTimestamp;
}

bool hostSupported(const wots4::ModContext* context) {
    if (!context || context->size < sizeof(*context) || context->version != wots4::modApiVersion || !context->host ||
        !context->module || !context->game)
        return false;
    const wots4::HostApi& api = *context->host;
    constexpr size_t requiredSize = offsetof(wots4::HostApi, showConversationMenu) + sizeof(api.showConversationMenu);
    return api.size >= requiredSize && api.version == wots4::modApiVersion && api.claim && api.releaseClaims && api.onFrame &&
        api.addConversationChoice && api.showConversationMenu;
}

}  // namespace

extern "C" BOOL WINAPI ModInitialize(const wots4::ModContext* context) {
    if (!hostSupported(context)) {
        if (context && context->host && context->host->log) {
            char message[224];
            sprintf_s(message,
                "[ERROR] This ModLoader does not offer Mod API version %u with conversation choices and frame callbacks; "
                "Boss Rush was refused. Use the ModLoader it was built for.",
                wots4::modApiVersion);
            context->host->log(context->module, message);
        }
        return FALSE;
    }
    host = context->host;
    module = context->module;
    boss_rush::game::setGameBase(reinterpret_cast<uintptr_t>(context->game));
    if (!exeSupported(context->game)) {
        log("[ERROR] This game exe is not the supported Steam version; Boss Rush was refused.");
        return FALSE;
    }
    const wots4::ConversationChoice fight{sizeof(wots4::ConversationChoice), ghostScript, ghostChoiceTexts,
        static_cast<uint32_t>(sizeof(ghostChoiceTexts) / sizeof(ghostChoiceTexts[0])), boss_rush::fightTheStrongLabel,
        wots4::conversationBeforeLast, fightLineShown, fightSelected, nullptr};
    if (!host->addConversationChoice(module, &fight) || !host->onFrame(module, onFrame, nullptr)) {
        log("[ERROR] The ModLoader did not accept the ghost's line or the frame callback; Boss Rush was refused.");
        return FALSE;
    }
    // Last, so that a refused initialization leaves no hook behind.
    boss_rush::installExitLock(*context, log);
    if (!boss_rush::installTravel(*context, log)) {
        // releaseClaims drops every reservation of the Mod, so the exit lock takes its own again.
        host->releaseClaims(module);
        boss_rush::reclaimExitLock(*context);
    }
    log("[INFO] Boss Rush initialized: the ghost of the graveyard offers 強者たちと戦う after the true route's ending.");
    return TRUE;
}
