#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#include <lodb/LoDB.h>
#include <stdint.h>
#include <string>
#include <vector>

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
                          uint64_t *authUserUuidOut = nullptr, std::string *cwdOut = nullptr);
    bool setSessionCwd(uint32_t nodeId, const char *cwd);
    std::vector<LoScalar> listUsers(const char *filterSubstr);
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
