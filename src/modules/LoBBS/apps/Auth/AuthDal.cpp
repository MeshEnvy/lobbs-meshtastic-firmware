#if !MESHTASTIC_EXCLUDE_LOBBS

#include "AuthDal.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSReply.h"
#include "AuthRecords.h"
#include "configuration.h"
#include "gps/RTC.h"
#include "mesh/NodeDB.h"
#include <SHA256.h>
#include <cctype>
#include <cstring>
#include <vector>

static void normalizeUsername(const char *username, char *normalized)
{
    size_t len = strlen(username);
    if (len > LOBBS_MAX_USERNAME_LEN)
        len = LOBBS_MAX_USERNAME_LEN;
    for (size_t i = 0; i < len; i++) {
        normalized[i] = tolower(username[i]);
    }
    normalized[len] = '\0';
}

AuthDal::AuthDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("users");
    lodb_.registerTable("sessions");
}

uint64_t AuthDal::userUuid(const LoScalar &user)
{
    uint64_t v = 0;
    user.getUint64(LODB_F_ID, v);
    return v;
}

bool AuthDal::userUsername(const LoScalar &user, char *buf, size_t bufCap)
{
    std::string name;
    if (!user.getString(AuthUser::FIELD_USERNAME, name) || bufCap == 0)
        return false;
    strncpy(buf, name.c_str(), bufCap - 1);
    buf[bufCap - 1] = '\0';
    return true;
}

bool AuthDal::userIsSysop(const LoScalar &user)
{
    bool v = false;
    return user.getBool(AuthUser::FIELD_SYSOP, v) && v;
}

static lodb_uuid_t usernameToUuid(const char *username)
{
    char normalized[LOBBS_USERNAME_BUFFER_SIZE];
    normalizeUsername(username, normalized);
    return lodb_new_uuid(normalized, nodeDB->getNodeNum());
}

static const char *stristrLocal(const char *haystack, const char *needle)
{
    if (!needle || !*needle)
        return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n)
            return haystack;
    }
    return nullptr;
}

static int compareUsersByName(const LoScalar &a, const LoScalar &b)
{
    char na[LOBBS_USERNAME_BUFFER_SIZE];
    char nb[LOBBS_USERNAME_BUFFER_SIZE];
    AuthDal::userUsername(a, na, sizeof(na));
    AuthDal::userUsername(b, nb, sizeof(nb));
    return strcasecmp(na, nb);
}

bool AuthDal::loadUserByUuid(uint64_t uuid, LoScalar *user)
{
    return lodb_.get("users", uuid, *user) == LODB_OK;
}

std::vector<LoScalar> AuthDal::listUsers(const char *filterSubstr)
{
    const bool hasFilter = filterSubstr && filterSubstr[0];
    return lodb_.select(
        "users",
        [filterSubstr, hasFilter](const LoScalar &rec) -> bool {
            if (!hasFilter)
                return true;
            char name[LOBBS_USERNAME_BUFFER_SIZE];
            AuthDal::userUsername(rec, name, sizeof(name));
            return stristrLocal(name, filterSubstr) != nullptr;
        },
        compareUsersByName);
}

bool AuthDal::buildUserList(const char *filterSubstr, std::string &msg, const char **emptyReply)
{
    const bool hasFilter = filterSubstr && filterSubstr[0];
    auto users = lodb_.select(
        "users",
        [filterSubstr, hasFilter](const LoScalar &rec) -> bool {
            if (!hasFilter)
                return true;
            char name[LOBBS_USERNAME_BUFFER_SIZE];
            AuthDal::userUsername(rec, name, sizeof(name));
            return stristrLocal(name, filterSubstr) != nullptr;
        },
        compareUsersByName);
    if (users.empty()) {
        *emptyReply = hasFilter ? "No users match filter." : "No users found";
        return false;
    }
    msg = "Users:\n";
    for (size_t i = 0; i < users.size(); i++) {
        char name[LOBBS_USERNAME_BUFFER_SIZE];
        AuthDal::userUsername(users[i], name, sizeof(name));
        if (i > 0)
            msg += ", ";
        msg += name;
        if (AuthDal::userIsSysop(users[i]))
            msg += "*";
    }
    return true;
}

bool AuthDal::formatUserListPage(const char *filterSubstr, uint32_t page1Based, std::string &msg, uint32_t &totalCount,
                                 const char **emptyReply)
{
    const bool hasFilter = filterSubstr && filterSubstr[0];
    auto users = lodb_.select(
        "users",
        [filterSubstr, hasFilter](const LoScalar &rec) -> bool {
            if (!hasFilter)
                return true;
            char name[LOBBS_USERNAME_BUFFER_SIZE];
            AuthDal::userUsername(rec, name, sizeof(name));
            return stristrLocal(name, filterSubstr) != nullptr;
        },
        compareUsersByName);
    if (users.empty()) {
        *emptyReply = hasFilter ? "No users match filter." : "No users found";
        totalCount = 0;
        return false;
    }
    totalCount = (uint32_t)users.size();
    std::vector<std::string> lines;
    lines.reserve(users.size());
    for (size_t i = 0; i < users.size(); i++) {
        char name[LOBBS_USERNAME_BUFFER_SIZE];
        AuthDal::userUsername(users[i], name, sizeof(name));
        std::string line = name;
        if (AuthDal::userIsSysop(users[i]))
            line += "*";
        lines.push_back(line);
    }

    std::vector<const char *> ptrs;
    ptrs.reserve(lines.size());
    for (auto &line : lines)
        ptrs.push_back(line.c_str());

    char buf[LOBBS_REPLY_BYTES + 1];
    const char *errEmpty = nullptr;
    const char *errBadPage = nullptr;
    if (!lobbsPagerFormatLines(buf, sizeof(buf), page1Based, ptrs.data(), (uint32_t)ptrs.size(), &errEmpty, &errBadPage)) {
        *emptyReply = errBadPage ? errBadPage : (hasFilter ? "No users match filter." : "No users found");
        return false;
    }
    msg = buf;
    return true;
}

bool AuthDal::isValidUsername(const char *username)
{
    if (!isalpha(username[0])) {
        return false;
    }
    for (size_t i = 1; username[i] != '\0'; i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            return false;
        }
    }
    return true;
}

bool AuthDal::isValidPassword(const char *password)
{
    for (size_t i = 0; password[i] != '\0'; i++) {
        char c = password[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '!' && c != '@' && c != '#' && c != '$' && c != '%') {
            return false;
        }
    }
    return true;
}

void AuthDal::hashPassword(const char *password, uint8_t *hash)
{
    SHA256 sha256;
    sha256.reset();
    sha256.update(password, strlen(password));
    sha256.finalize(hash, 32);
}

bool AuthDal::loadUserByUsername(const char *username, LoScalar *user)
{
    lodb_uuid_t userUuid = usernameToUuid(username);
    LoDbError err = lodb_.get("users", userUuid, *user);
    if (err == LODB_OK) {
        LOG_DEBUG("Loaded user by username: %s", username);
        return true;
    }
    LOG_DEBUG("User not found: %s", username);
    return false;
}

static void dropSession(LoDb &lodb, uint32_t sessionKey)
{
    lodb.deleteRecord("sessions", (lodb_uuid_t)sessionKey);
}

bool AuthDal::loadUserByNodeId(uint32_t nodeId, LoScalar *user, uint32_t *sessionNodeIdOut, uint64_t *authUserUuidOut)
{
    LoScalar session;
    if (lodb_.get("sessions", (lodb_uuid_t)nodeId, session) != LODB_OK) {
        LOG_DEBUG("No session found for node 0x%08x", nodeId);
        return false;
    }

    uint64_t userUuid = 0;
    if (!session.getUint64(AuthSession::FIELD_USER_UUID, userUuid) || userUuid == 0) {
        LOG_WARN("Invalid session at 0x%08x, removing", nodeId);
        dropSession(lodb_, nodeId);
        return false;
    }

    uint32_t sessionNodeId = nodeId;
    uint32_t storedNode = 0;
    if (session.getUint32(AuthSession::FIELD_NODE_ID, storedNode) && storedNode != 0)
        sessionNodeId = storedNode;

    if (lodb_.get("users", userUuid, *user) != LODB_OK) {
        LOG_WARN("Session 0x%08x references missing user, removing session", sessionNodeId);
        dropSession(lodb_, sessionNodeId);
        return false;
    }

    LOG_DEBUG("Loaded user by node ID: 0x%08x -> UUID: " LODB_UUID_FMT, sessionNodeId, LODB_UUID_ARGS(userUuid));
    if (sessionNodeIdOut)
        *sessionNodeIdOut = sessionNodeId;
    if (authUserUuidOut)
        *authUserUuidOut = userUuid;
    return true;
}

bool AuthDal::createUser(const char *username, const char *password, uint32_t nodeId)
{
    lodb_uuid_t userUuid = usernameToUuid(username);
    bool isFirstUser = (lodb_.count("users") == 0);

    LoScalar user;
    user.setString(AuthUser::FIELD_USERNAME, username);
    uint8_t hash[32];
    hashPassword(password, hash);
    user.setBytesHex(AuthUser::FIELD_PASSWORD, hash, 32);
    user.setBool(AuthUser::FIELD_SYSOP, isFirstUser);
    LoDbError err = lodb_.insert("users", userUuid, user);
    if (err != LODB_OK) {
        LOG_ERROR("Failed to create user: %s", username);
        return false;
    }

    LOG_INFO("Created user: %s (sysop: %s)", username, isFirstUser ? "yes" : "no");
    return loginUser(username, nodeId);
}

bool AuthDal::verifyPassword(const LoScalar *user, const char *password)
{
    uint8_t stored[32];
    size_t n = 0;
    if (!user->getBytesHex(AuthUser::FIELD_PASSWORD, stored, sizeof(stored), n) || n != 32)
        return false;
    uint8_t providedHash[32];
    hashPassword(password, providedHash);
    return memcmp(stored, providedHash, 32) == 0;
}

bool AuthDal::loginUser(const char *username, uint32_t nodeId)
{
    LoScalar session;
    session.setUint64(AuthSession::FIELD_USER_UUID, usernameToUuid(username));
    session.setUint32(AuthSession::FIELD_NODE_ID, nodeId);

    lodb_uuid_t sessionUuid = (lodb_uuid_t)nodeId;
    lodb_.deleteRecord("sessions", sessionUuid);

    LoDbError err = lodb_.insert("sessions", sessionUuid, session);
    if (err != LODB_OK) {
        LOG_ERROR("Failed to create session for node 0x%08x", nodeId);
        return false;
    }

    LOG_INFO("Created session for user %s on node 0x%08x", username, nodeId);
    return true;
}

bool AuthDal::logoutUser(uint32_t nodeId)
{
    lodb_uuid_t sessionUuid = (lodb_uuid_t)nodeId;
    LoDbError err = lodb_.deleteRecord("sessions", sessionUuid);
    if (err == LODB_OK) {
        LOG_INFO("Logged out node 0x%08x", nodeId);
        return true;
    }
    LOG_WARN("No session found to log out for node 0x%08x", nodeId);
    return false;
}

uint64_t AuthDal::getUserUuidByUsername(const char *username)
{
    uint64_t userUuid = usernameToUuid(username);
    LoScalar user;
    LoDbError err = lodb_.get("users", userUuid, user);
    if (err == LODB_OK)
        return userUuid;
    return 0;
}

bool AuthDal::setUserSysopByUsername(const char *username, bool isSysop)
{
    LoScalar user;
    if (!loadUserByUsername(username, &user))
        return false;
    uint64_t uuid = userUuid(user);
    user.setBool(AuthUser::FIELD_SYSOP, isSysop);
    user.removeField(LODB_F_CREATED);
    user.removeField(LODB_F_UPDATED);
    return lodb_.update("users", uuid, user) == LODB_OK;
}

bool AuthDal::setPasswordByUsername(const char *username, const char *password)
{
    LoScalar user;
    if (!loadUserByUsername(username, &user))
        return false;
    uint64_t uuid = userUuid(user);
    uint8_t hash[32];
    hashPassword(password, hash);
    user.setBytesHex(AuthUser::FIELD_PASSWORD, hash, 32);
    user.removeField(LODB_F_CREATED);
    user.removeField(LODB_F_UPDATED);
    return lodb_.update("users", uuid, user) == LODB_OK;
}

uint32_t AuthDal::countSysopUsers()
{
    return (uint32_t)lodb_.count("users", [](const LoScalar &rec) -> bool { return AuthDal::userIsSysop(rec); });
}

uint32_t AuthDal::countAllUsers()
{
    int n = lodb_.count("users");
    return n < 0 ? 0 : (uint32_t)n;
}

bool AuthDal::kickUserByUsername(const char *username)
{
    LoScalar user;
    if (!loadUserByUsername(username, &user))
        return false;
    uint64_t userUuidVal = userUuid(user);

    auto sessions = lodb_.select(
        "sessions",
        [userUuidVal](const LoScalar &rec) -> bool {
            uint64_t u = 0;
            return rec.getUint64(AuthSession::FIELD_USER_UUID, u) && u == userUuidVal;
        },
        LoDbComparator());

    for (const auto &rec : sessions) {
        uint32_t nodeId = 0;
        if (rec.getUint32(AuthSession::FIELD_NODE_ID, nodeId))
            lodb_.deleteRecord("sessions", (lodb_uuid_t)nodeId);
    }
    return true;
}

#endif
