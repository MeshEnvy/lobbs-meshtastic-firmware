#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "news.pb.h"
#include <lodb/LoDB.h>
#include <stdint.h>
#include <vector>

struct LoBBSNewsEntry {
    meshtastic_LoBBSNews *news;
    bool isRead;
};

class NewsDal
{
  public:
    explicit NewsDal(LoDb &lodb);

    bool postNews(uint64_t authorUserUuid, const char *message);
    std::vector<LoBBSNewsEntry> getNewsForUser(uint64_t userUuid, uint32_t offset, uint32_t limit);
    std::vector<LoBBSNewsEntry> getAllNewsForUser(uint64_t userUuid);
    bool markNewsAsRead(uint64_t newsUuid, uint64_t userUuid);
    bool markNewsAsUnread(uint64_t newsUuid, uint64_t userUuid);
    uint16_t countUnreadNews(uint64_t userUuid);
    bool deleteNewsUuid(uint64_t newsUuid);
    bool deleteNewsListIndex(uint64_t readerUuid, uint32_t oneBasedIndex);

  private:
    bool isNewsReadByUser(uint64_t newsUuid, uint64_t userUuid);
    LoDb &lodb_;
};

#endif
