#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#include "auth.pb.h"
#include <lodb/LoDB.h>
#include <stdint.h>
#include <string>

class AuthDal
{
  public:
    AuthDal(LoDb &lodb);

    bool isValidUsername(const char *username);
    bool isValidPassword(const char *password);
    bool loadUserByUsername(const char *username, meshtastic_LoBBSUser *user);
    bool loadUserByUuid(uint64_t uuid, meshtastic_LoBBSUser *user);
    bool loadUserByNodeId(uint32_t nodeId, meshtastic_LoBBSUser *user);
    /** filterSubstr may be null or empty for all users. On false, *emptyReply is the user-facing message. */
    bool buildUserList(const char *filterSubstr, std::string &msg, const char **emptyReply);
    /** Paginated user names (one per line in reply). page1Based >= 1. Sets totalCount. On false, *emptyReply set. */
    bool formatUserListPage(const char *filterSubstr, uint32_t page1Based, std::string &msg, uint32_t &totalCount,
                            const char **emptyReply);
    bool createUser(const char *username, const char *password, uint32_t nodeId);
    bool verifyPassword(const meshtastic_LoBBSUser *user, const char *password);
    bool loginUser(const char *username, uint32_t nodeId);
    bool logoutUser(uint32_t nodeId);
    uint64_t getUserUuidByUsername(const char *username);

    bool setUserSysopByUsername(const char *username, bool isSysop);
    bool setPasswordByUsername(const char *username, const char *password);
    uint32_t countSysopUsers();
    uint32_t countAllUsers();
    bool kickUserByUsername(const char *username);

  private:
    static void hashPassword(const char *password, uint8_t *hash);
    LoDb &lodb_;
};

#endif
