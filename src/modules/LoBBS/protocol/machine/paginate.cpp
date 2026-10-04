#if !MESHTASTIC_EXCLUDE_LOBBS

#include "paginate.h"
#include "LoBBSReply.h"
#include <cstdio>

bool lobbsPaginateMachine(uint32_t reqId, const std::string &document, uint32_t page1, std::string &pageOut, const char **errMsg)
{
    pageOut.clear();
    if (page1 == 0)
        page1 = 1;

    char prefix[32];
    size_t prefixLen = (size_t)snprintf(prefix, sizeof(prefix), "<%u>ok\n", reqId);
    if (prefixLen + document.size() <= LOBBS_REPLY_BYTES) {
        if (page1 > 1) {
            if (errMsg)
                *errMsg = "No such page.";
            return false;
        }
        pageOut = prefix;
        pageOut += document;
        return true;
    }

    size_t offset = 0;
    for (uint32_t n = 1; offset < document.size(); n++) {
        // Budget assumes the `!` so the last fragment never overflows.
        prefixLen = (size_t)snprintf(prefix, sizeof(prefix), "<%u:%u!>ok\n", reqId, n);
        size_t budget = LOBBS_REPLY_BYTES - prefixLen;
        size_t remaining = document.size() - offset;
        bool last = remaining <= budget;
        size_t take = last ? remaining : budget;
        if (n == page1) {
            snprintf(prefix, sizeof(prefix), "<%u:%u%s>ok\n", reqId, n, last ? "!" : "");
            pageOut = prefix;
            pageOut.append(document, offset, take);
            return true;
        }
        offset += take;
    }
    if (errMsg)
        *errMsg = "No such page.";
    return false;
}

#endif
