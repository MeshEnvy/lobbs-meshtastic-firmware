#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSDb.h"
#include <stdint.h>
#include <vector>

class MailDal
{
  public:
    static void registerTables(LoDb &db);

    explicit MailDal(LoBBSDb &db) : db_(db) {}

    bool sendMail(uint64_t fromUserUuid, uint64_t toUserUuid, const char *message);
    std::vector<void *> getMailForUser(uint64_t userUuid, uint32_t offset, uint32_t limit);
    std::vector<void *> getAllMailForUser(uint64_t userUuid);
    bool markMailAsRead(uint64_t mailUuid);
    uint16_t countUnreadMail(uint64_t userUuid);
    bool deleteMailUuid(uint64_t mailUuid);
    bool deleteMailInboxIndex(uint64_t inboxOwnerUuid, uint32_t oneBasedIndex);

  private:
    LoBBSDb &db_;
    LoDb *lodb() { return db_.getDb(); }
};

#endif
