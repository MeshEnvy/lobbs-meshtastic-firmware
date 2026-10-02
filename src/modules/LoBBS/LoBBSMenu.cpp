#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSMenu.h"
#include "apps/Mail/Mail.h"
#include "apps/News/News.h"
#include "apps/Root/Root.h"
#include "LoBBSHistory.h"
#include "LoBBSModule.h"
#include "LoBBSPaging.h"

static constexpr size_t LOBBS_MENU_MAX_SLOTS = 4;

struct LobbsMenuSlot {
    uint32_t sessionNodeId = 0;
    bool occupied = false;
    LobbsHistory hist;
};

static LobbsMenuSlot lobbsMenuSlots[LOBBS_MENU_MAX_SLOTS];

static LobbsMenuSlot *findSlot(uint32_t sessionNodeId)
{
    for (auto &s : lobbsMenuSlots) {
        if (s.occupied && s.sessionNodeId == sessionNodeId)
            return &s;
    }
    return nullptr;
}

static LobbsMenuSlot *allocSlot(uint32_t sessionNodeId)
{
    if (LobbsMenuSlot *e = findSlot(sessionNodeId))
        return e;
    for (auto &s : lobbsMenuSlots) {
        if (!s.occupied) {
            s = LobbsMenuSlot();
            s.sessionNodeId = sessionNodeId;
            s.occupied = true;
            return &s;
        }
    }
    lobbsMenuSlots[0].occupied = false;
    for (size_t i = 1; i < LOBBS_MENU_MAX_SLOTS; i++)
        lobbsMenuSlots[i - 1] = lobbsMenuSlots[i];
    LobbsMenuSlot &s = lobbsMenuSlots[LOBBS_MENU_MAX_SLOTS - 1];
    s = LobbsMenuSlot();
    s.sessionNodeId = sessionNodeId;
    s.occupied = true;
    return &s;
}

static void runLine(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, bool isAuth,
                    const meshtastic_LoBBSUser *user, bool isAdmin, const char *line)
{
    LobbsMenuSlot *slot = allocSlot(sessionNodeId);
    LobbsCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.sessionNodeId = sessionNodeId;
    ctx.isAuth = isAuth;
    ctx.user = user;
    ctx.isAdmin = isAdmin;
    lobbsHistoryHandle(&slot->hist, &ctx, line);
}

LobbsMenuKeyResult lobbsMenuTryGlobalKeys(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                                          bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin, const char *line)
{
    runLine(mod, mp, sessionNodeId, isAuth, user, isAdmin, line);
    return LobbsMenuKeyResult::Handled;
}

void lobbsMenuHandleLine(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, bool isAuth,
                         const meshtastic_LoBBSUser *user, bool isAdmin, const char *line)
{
    runLine(mod, mp, sessionNodeId, isAuth, user, isAdmin, line);
}

void lobbsMenuReprint(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, bool isAuth,
                      const meshtastic_LoBBSUser *user, bool isAdmin)
{
    runLine(mod, mp, sessionNodeId, isAuth, user, isAdmin, "?");
}

void lobbsMenuOnLogout(uint32_t sessionNodeId)
{
    LobbsMenuSlot *slot = findSlot(sessionNodeId);
    if (!slot)
        return;
    slot->hist.depth = 0;
    slot->hist.scratch[0] = '\0';
    slot->hist.scratch2[0] = '\0';
    lobbsPageClearUser(sessionNodeId);
}

void lobbsMenuAfterMailList(uint32_t sessionNodeId, uint64_t inboxUuid)
{
    (void)sessionNodeId;
    (void)inboxUuid;
}

void lobbsMenuShowMailList(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint64_t inboxUuid)
{
    LobbsMenuSlot *slot = allocSlot(sessionNodeId);
    LobbsCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.sessionNodeId = sessionNodeId;
    ctx.isAuth = user != nullptr;
    ctx.user = user;
    ctx.isAdmin = isAdmin;
    slot->hist.ctx = &ctx;
    slot->hist.drew = false;
    lobbsRootInstall(&slot->hist);
    lobbsMailPush(&slot->hist);
    lobbsMailPushInbox(&slot->hist, inboxUuid);
    lobbsHistoryDraw(&slot->hist);
}

void lobbsMenuShowMailRead(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint64_t inboxUuid, uint32_t idx)
{
    LobbsMenuSlot *slot = allocSlot(sessionNodeId);
    LobbsCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.sessionNodeId = sessionNodeId;
    ctx.isAuth = user != nullptr;
    ctx.user = user;
    ctx.isAdmin = isAdmin;
    slot->hist.ctx = &ctx;
    slot->hist.drew = false;
    lobbsRootInstall(&slot->hist);
    lobbsMailPush(&slot->hist);
    lobbsMailPushInbox(&slot->hist, inboxUuid);
    lobbsMailPushRead(&slot->hist, inboxUuid, idx);
    lobbsHistoryDraw(&slot->hist);
}

void lobbsMenuShowNewsList(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin)
{
    LobbsMenuSlot *slot = allocSlot(sessionNodeId);
    LobbsCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.sessionNodeId = sessionNodeId;
    ctx.isAuth = user != nullptr;
    ctx.user = user;
    ctx.isAdmin = isAdmin;
    slot->hist.ctx = &ctx;
    slot->hist.drew = false;
    lobbsRootInstall(&slot->hist);
    lobbsNewsPush(&slot->hist);
    lobbsNewsPushList(&slot->hist);
    lobbsHistoryDraw(&slot->hist);
}

void lobbsMenuShowNewsRead(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint32_t idx)
{
    LobbsMenuSlot *slot = allocSlot(sessionNodeId);
    LobbsCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.sessionNodeId = sessionNodeId;
    ctx.isAuth = user != nullptr;
    ctx.user = user;
    ctx.isAdmin = isAdmin;
    slot->hist.ctx = &ctx;
    slot->hist.drew = false;
    lobbsRootInstall(&slot->hist);
    lobbsNewsPush(&slot->hist);
    lobbsNewsPushList(&slot->hist);
    lobbsNewsPushRead(&slot->hist, idx);
    lobbsHistoryDraw(&slot->hist);
}

void lobbsMenuAfterNewsList(uint32_t sessionNodeId)
{
    (void)sessionNodeId;
}

void lobbsMenuAfterUserList(uint32_t sessionNodeId)
{
    (void)sessionNodeId;
}

#endif
