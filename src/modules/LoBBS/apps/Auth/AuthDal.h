#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSDb.h"
#include "../../lobbs.pb.h"

class AuthDal
{
  public:
    static void registerTables(LoDb &db);

    explicit AuthDal(LoBBSDb &db) : db_(db) {}

    bool isValidUsername(const char *username);
    bool isValidPassword(const char *password);
    bool loadUserByUsername(const char *username, meshtastic_LoBBSUser *user);
    bool loadUserByNodeId(uint32_t nodeId, meshtastic_LoBBSUser *user);
    bool createUser(const char *username, const char *password, uint32_t nodeId);
    bool verifyPassword(const meshtastic_LoBBSUser *user, const char *password);
    bool loginUser(const char *username, uint32_t nodeId);
    bool logoutUser(uint32_t nodeId);
    uint64_t getUserUuidByUsername(const char *username);

    bool setUserAdminByUsername(const char *username, bool isAdmin);
    uint32_t countAdminUsers();
    bool kickUserByUsername(const char *username);

  private:
    static void hashPassword(const char *password, uint8_t *hash);
    LoDb *lodb() { return db_.getDb(); }
    LoBBSDb &db_;
};

#endif
