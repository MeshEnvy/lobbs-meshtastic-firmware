#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSPaging.h"
#include <cstdio>
#include <cstring>

struct LobbsPageResult {
    char pages[LOBBS_MAX_PAGES][LOBBS_PAGE_BYTES + 1];
    uint8_t count = 0;
    uint8_t lastSent = 0;
};

struct LobbsPageSlot {
    uint32_t sessionNodeId = 0;
    LobbsPageResult result;
    bool occupied = false;
};

static LobbsPageSlot lobbsPageSlots[LOBBS_MAX_PAGE_USERS];
static char lobbsPageErr[64];

static LobbsPageSlot *lobbsFindSlot(uint32_t sessionNodeId)
{
    for (auto &slot : lobbsPageSlots) {
        if (slot.occupied && slot.sessionNodeId == sessionNodeId)
            return &slot;
    }
    return nullptr;
}

static LobbsPageSlot *lobbsAllocSlot(uint32_t sessionNodeId)
{
    LobbsPageSlot *existing = lobbsFindSlot(sessionNodeId);
    if (existing)
        return existing;

    for (auto &slot : lobbsPageSlots) {
        if (!slot.occupied) {
            slot.sessionNodeId = sessionNodeId;
            slot.occupied = true;
            slot.result.count = 0;
            slot.result.lastSent = 0;
            return &slot;
        }
    }
    lobbsPageSlots[0].occupied = false;
    for (int i = 1; i < LOBBS_MAX_PAGE_USERS; i++) {
        lobbsPageSlots[i - 1] = lobbsPageSlots[i];
    }
    LobbsPageSlot &slot = lobbsPageSlots[LOBBS_MAX_PAGE_USERS - 1];
    slot.sessionNodeId = sessionNodeId;
    slot.occupied = true;
    slot.result.count = 0;
    slot.result.lastSent = 0;
    return &slot;
}

static bool lobbsSplitIntoPages(const char *body, LobbsPageResult &out, const char *&errMsg)
{
    out.count = 0;
    if (!body)
        body = "";
    size_t bodyLen = strlen(body);
    size_t pos = 0;
    if (bodyLen == 0) {
        out.pages[0][0] = '\0';
        out.count = 1;
        return true;
    }
    while (pos < bodyLen) {
        if (out.count >= LOBBS_MAX_PAGES) {
            errMsg = "Too many results. Narrow with a filter.";
            return false;
        }
        size_t remaining = bodyLen - pos;
        size_t chunk = remaining < LOBBS_PAGE_BYTES ? remaining : LOBBS_PAGE_BYTES;
        if (pos + chunk < bodyLen) {
            size_t nl = 0;
            for (size_t i = chunk; i > 0; i--) {
                if (body[pos + i - 1] == '\n') {
                    nl = i;
                    break;
                }
            }
            if (nl > 0)
                chunk = nl;
        }
        if (chunk == 0)
            chunk = 1;
        memcpy(out.pages[out.count], body + pos, chunk);
        out.pages[out.count][chunk] = '\0';
        out.count++;
        pos += chunk;
    }
    return true;
}

void lobbsPageClearUser(uint32_t sessionNodeId)
{
    LobbsPageSlot *slot = lobbsFindSlot(sessionNodeId);
    if (!slot)
        return;
    slot->result.count = 0;
    slot->result.lastSent = 0;
    slot->occupied = false;
}

bool lobbsPageStoreAndFirst(uint32_t sessionNodeId, const char *body, const char *&outPage, const char *&errMsg)
{
    LobbsPageSlot *slot = lobbsAllocSlot(sessionNodeId);
    if (!lobbsSplitIntoPages(body, slot->result, errMsg)) {
        slot->result.count = 0;
        return false;
    }
    slot->result.lastSent = 1;
    outPage = slot->result.pages[0];
    return true;
}

bool lobbsPageFetch(uint32_t sessionNodeId, int pageNum, const char *&outPage, const char *&errMsg)
{
    LobbsPageSlot *slot = lobbsFindSlot(sessionNodeId);
    if (!slot || !slot->occupied || slot->result.count == 0) {
        errMsg = "Page history not available";
        return false;
    }

    uint8_t target = 0;
    if (pageNum <= 0) {
        if (slot->result.lastSent >= slot->result.count) {
            snprintf(lobbsPageErr, sizeof(lobbsPageErr), "Page out of range (1-%u)", slot->result.count);
            errMsg = lobbsPageErr;
            return false;
        }
        target = slot->result.lastSent + 1;
    } else {
        target = (uint8_t)pageNum;
    }

    if (target < 1 || target > slot->result.count) {
        snprintf(lobbsPageErr, sizeof(lobbsPageErr), "Page out of range (1-%u)", slot->result.count);
        errMsg = lobbsPageErr;
        return false;
    }

    slot->result.lastSent = target;
    outPage = slot->result.pages[target - 1];
    return true;
}

#endif
