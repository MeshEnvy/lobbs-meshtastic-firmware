#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSPaging.h"
#include <cstdio>
#include <cstring>
#include <vector>

struct LobbsPageResult {
    std::string pages[LOBBS_MAX_PAGES];
    uint8_t count = 0;
    uint8_t lastSent = 0;
};

struct LobbsPageSlot {
    uint32_t sessionNodeId = 0;
    LobbsPageResult result;
    bool occupied = false;
};

static LobbsPageSlot lobbsPageSlots[LOBBS_MAX_PAGE_USERS];

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
            slot.result = LobbsPageResult();
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
    slot.result = LobbsPageResult();
    return &slot;
}

static bool lobbsSplitIntoPages(const std::string &body, LobbsPageResult &out, std::string &errMsg)
{
    out.count = 0;
    const size_t maxChunk = LOBBS_PAGE_BYTES;
    size_t pos = 0;
    while (pos < body.size()) {
        if (out.count >= LOBBS_MAX_PAGES) {
            errMsg = "Too many results. Narrow with a filter.";
            return false;
        }
        size_t remaining = body.size() - pos;
        size_t chunk = remaining < maxChunk ? remaining : maxChunk;
        if (pos + chunk < body.size()) {
            size_t lastNl = body.rfind('\n', pos + chunk - 1);
            if (lastNl != std::string::npos && lastNl >= pos)
                chunk = lastNl - pos + 1;
        }
        out.pages[out.count++] = body.substr(pos, chunk);
        pos += chunk;
    }
    return true;
}

void lobbsPageClearUser(uint32_t sessionNodeId)
{
    LobbsPageSlot *slot = lobbsFindSlot(sessionNodeId);
    if (slot)
        slot->occupied = false;
}

bool lobbsPageStoreAndFirst(uint32_t sessionNodeId, const std::string &body, std::string &outPage, std::string &errMsg)
{
    LobbsPageResult split;
    if (!lobbsSplitIntoPages(body, split, errMsg))
        return false;

    LobbsPageSlot *slot = lobbsAllocSlot(sessionNodeId);
    slot->result = split;
    slot->result.lastSent = 1;
    outPage = slot->result.pages[0];
    return true;
}

bool lobbsPageFetch(uint32_t sessionNodeId, int pageNum, std::string &outPage, std::string &errMsg)
{
    LobbsPageSlot *slot = lobbsFindSlot(sessionNodeId);
    if (!slot || !slot->occupied || slot->result.count == 0) {
        errMsg = "Page history not available";
        return false;
    }

    uint8_t target = 0;
    if (pageNum <= 0) {
        if (slot->result.lastSent >= slot->result.count) {
            errMsg = "Page out of range (1-" + std::to_string(slot->result.count) + ")";
            return false;
        }
        target = slot->result.lastSent + 1;
    } else {
        target = (uint8_t)pageNum;
    }

    if (target < 1 || target > slot->result.count) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Page out of range (1-%u)", slot->result.count);
        errMsg = buf;
        return false;
    }

    slot->result.lastSent = target;
    outPage = slot->result.pages[target - 1];
    return true;
}

#endif
