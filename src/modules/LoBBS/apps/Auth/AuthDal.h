#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#include <lodb/LoDB.h>
#include <stdint.h>
#include <string>

class AuthDal
{
  public:
    AuthDal(LoDb &lodb);

    static uint64_t userUuid(const LoScalar &user);
    static bool userUsername(const LoScalar &user, char *buf, size_t bufCap);
    static bool userIsSysop(const LoScalar &user);

    bool isValidUsername(const char *username);
    bool isValidPassword(const char *password);
    bool loadUserByUsername(const char *username, LoScalar *user);
    bool loadUserByUuid(uint64_t uuid, LoScalar *user);
    bool loadUserByNodeId(uint32_t nodeId, LoScalar *user, uint32_t *sessionNodeIdOut = nullptr,
                          uint64_t *authUserUuidOut = nullptr);
    bool buildUserList(const char *filterSubstr, std::string &msg, const char **emptyReply);
    bool formatUserListPage(const char *filterSubstr, uint32_t page1Based, std::string &msg, uint32_t &totalCount,
                            const char **emptyReply);
    bool createUser(const char *username, const char *password, uint32_t nodeId);
    bool verifyPassword(const LoScalar *user, const char *password);
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
