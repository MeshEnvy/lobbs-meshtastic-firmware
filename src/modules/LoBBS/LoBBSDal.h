#pragma once

#include "lobbs.pb.h"
#include <lodb/LoDB.h>
#include <stdint.h>
#include <vector>

struct LoBBSNewsEntry {
    meshtastic_LoBBSNews *news;
    bool isRead;
};

#define LOBBS_MAX_USERNAME_LEN 32
#define LOBBS_USERNAME_BUFFER_SIZE (LOBBS_MAX_USERNAME_LEN + 1)
#define LOBBS_XSTR(x) LOBBS_STR(x)
#define LOBBS_STR(x) #x

class LoBBSDal
{
  public:
    LoBBSDal(uint32_t hostNodeId);
    ~LoBBSDal();

    bool isValidUsername(const char *username);
    bool isValidPassword(const char *password);
    bool loadUserByUsername(const char *username, meshtastic_LoBBSUser *user);
    bool loadUserByNodeId(uint32_t nodeId, meshtastic_LoBBSUser *user);
    bool createUser(const char *username, const char *password, uint32_t nodeId);
    bool verifyPassword(const meshtastic_LoBBSUser *user, const char *password);
    bool loginUser(const char *username, uint32_t nodeId);
    bool logoutUser(uint32_t nodeId);
    uint64_t getUserUuidByUsername(const char *username);
    bool sendMail(uint64_t fromUserUuid, uint64_t toUserUuid, const char *message);
    std::vector<void *> getMailForUser(uint64_t userUuid, uint32_t offset, uint32_t limit);
    bool markMailAsRead(uint64_t mailUuid);
    bool postNews(uint64_t authorUserUuid, const char *message);
    std::vector<LoBBSNewsEntry> getNewsForUser(uint64_t userUuid, uint32_t offset, uint32_t limit);
    bool isNewsReadByUser(uint64_t newsUuid, uint64_t userUuid);
    bool markNewsAsRead(uint64_t newsUuid, uint64_t userUuid);
    LoDb *getDb() { return db; }

  private:
    static void hashPassword(const char *password, uint8_t *hash);
    LoDb *db;
    uint32_t hostNodeId;
};
