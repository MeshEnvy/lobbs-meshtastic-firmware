#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t LOBBS_MAX_PAGES = 10;
static constexpr uint8_t LOBBS_MAX_PAGE_USERS = 4;
static constexpr size_t LOBBS_PAGE_BYTES = 200;

// Split body into pages (<= LOBBS_MAX_PAGES). On success stores for sessionNodeId.
// outPage points at page 1 in the session slot (valid until the next store/clear for that session).
bool lobbsPageStoreAndFirst(uint32_t sessionNodeId, const char *body, const char *&outPage, const char *&errMsg);

// pageNum 0 = next page after lastSent; pageNum > 0 = that page (1-based).
// outPage points into the session slot.
bool lobbsPageFetch(uint32_t sessionNodeId, int pageNum, const char *&outPage, const char *&errMsg);

void lobbsPageClearUser(uint32_t sessionNodeId);

#endif
