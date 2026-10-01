#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <stdint.h>
#include <string>

static constexpr uint8_t LOBBS_MAX_PAGES = 10;
static constexpr uint8_t LOBBS_MAX_PAGE_USERS = 4;
static constexpr size_t LOBBS_PAGE_BYTES = 200;

// Split body into pages (<= LOBBS_MAX_PAGES). On success stores for sessionNodeId and returns page 1 in outPage.
bool lobbsPageStoreAndFirst(uint32_t sessionNodeId, const std::string &body, std::string &outPage, std::string &errMsg);

// pageNum 0 = next page after lastSent; pageNum > 0 = that page (1-based).
bool lobbsPageFetch(uint32_t sessionNodeId, int pageNum, std::string &outPage, std::string &errMsg);

void lobbsPageClearUser(uint32_t sessionNodeId);

#endif
