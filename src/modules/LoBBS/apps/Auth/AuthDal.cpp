#if !MESHTASTIC_EXCLUDE_LOBBS

#include "AuthDal.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSReply.h"
#include "auth.pb.h"
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
    lodb_.registerTable("users", &meshtastic_LoBBSUser_msg, sizeof(meshtastic_LoBBSUser));
    lodb_.registerTable("sessions", &meshtastic_LoBBSSession_msg, sizeof(meshtastic_LoBBSSession));
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

static int compareUsersByName(const void *a, const void *b)
{
    return strcasecmp(((const meshtastic_LoBBSUser *)a)->username, ((const meshtastic_LoBBSUser *)b)->username);
}

bool AuthDal::loadUserByUuid(uint64_t uuid, meshtastic_LoBBSUser *user)
{
    return lodb_.get("users", uuid, user) == LODB_OK;
}

bool AuthDal::buildUserList(const char *filterSubstr, std::string &msg, const char **emptyReply)
{
    const bool hasFilter = filterSubstr && filterSubstr[0];
    auto users = lodb_.select(
        "users",
        [filterSubstr, hasFilter](const void *rec) -> bool {
            const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
            if (!hasFilter)
                return true;
            return stristrLocal(u->username, filterSubstr) != nullptr;
        },
        compareUsersByName);
    if (users.empty()) {
        LoDb::freeRecords(users);
        *emptyReply = hasFilter ? "No users match filter." : "No users found";
        return false;
    }
    msg = "Users:\n";
    for (size_t i = 0; i < users.size(); i++) {
        const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)users[i];
        if (i > 0)
            msg += ", ";
        msg += u->username;
        if (u->is_admin)
            msg += "*";
    }
    LoDb::freeRecords(users);
    return true;
}

bool AuthDal::formatUserListPage(const char *filterSubstr, uint32_t page1Based, std::string &msg, uint32_t &totalCount,
                                 const char **emptyReply)
{
    const bool hasFilter = filterSubstr && filterSubstr[0];
    auto users = lodb_.select(
        "users",
        [filterSubstr, hasFilter](const void *rec) -> bool {
            const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
            if (!hasFilter)
                return true;
            return stristrLocal(u->username, filterSubstr) != nullptr;
        },
        compareUsersByName);
    if (users.empty()) {
        LoDb::freeRecords(users);
        *emptyReply = hasFilter ? "No users match filter." : "No users found";
        totalCount = 0;
        return false;
    }
    totalCount = (uint32_t)users.size();
    std::vector<std::string> lines;
    lines.reserve(users.size());
    for (size_t i = 0; i < users.size(); i++) {
        const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)users[i];
        std::string line = u->username;
        if (u->is_admin)
            line += "*";
        lines.push_back(line);
    }
    LoDb::freeRecords(users);

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

bool AuthDal::loadUserByUsername(const char *username, meshtastic_LoBBSUser *user)
{
    lodb_uuid_t userUuid = usernameToUuid(username);
    LoDbError err = lodb_.get("users", userUuid, user);
    if (err == LODB_OK) {
        LOG_DEBUG("Loaded user by username: %s", username);
        return true;
    }
    LOG_DEBUG("User not found: %s", username);
    return false;
}

bool AuthDal::loadUserByNodeId(uint32_t nodeId, meshtastic_LoBBSUser *user)
{
    lodb_uuid_t sessionUuid = (lodb_uuid_t)nodeId;
    meshtastic_LoBBSSession session = meshtastic_LoBBSSession_init_zero;
    LoDbError err = lodb_.get("sessions", sessionUuid, &session);
    if (err != LODB_OK) {
        LOG_DEBUG("No session found for node 0x%08x", nodeId);
        return false;
    }
    err = lodb_.get("users", session.user_uuid, user);
    if (err == LODB_OK) {
        LOG_DEBUG("Loaded user by node ID: 0x%08x -> UUID: " LODB_UUID_FMT, nodeId, LODB_UUID_ARGS(session.user_uuid));
        return true;
    }
    return false;
}

bool AuthDal::createUser(const char *username, const char *password, uint32_t nodeId)
{
    lodb_uuid_t userUuid = usernameToUuid(username);
    bool isFirstUser = (lodb_.count("users") == 0);

    meshtastic_LoBBSUser user = meshtastic_LoBBSUser_init_zero;
    strncpy(user.username, username, sizeof(user.username) - 1);
    user.uuid = userUuid;
    user.password_hash.size = 32;
    hashPassword(password, user.password_hash.bytes);
    user.is_admin = isFirstUser;
    LoDbError err = lodb_.insert("users", userUuid, &user);
    if (err != LODB_OK) {
        LOG_ERROR("Failed to create user: %s", username);
        return false;
    }

    LOG_INFO("Created user: %s (admin: %s)", username, isFirstUser ? "yes" : "no");
    return loginUser(username, nodeId);
}

bool AuthDal::verifyPassword(const meshtastic_LoBBSUser *user, const char *password)
{
    uint8_t providedHash[32];
    hashPassword(password, providedHash);
    return memcmp(user->password_hash.bytes, providedHash, 32) == 0;
}

bool AuthDal::loginUser(const char *username, uint32_t nodeId)
{
    meshtastic_LoBBSSession session = meshtastic_LoBBSSession_init_zero;
    session.user_uuid = usernameToUuid(username);
    session.node_id = nodeId;
    session.last_login_time = getTime();

    lodb_uuid_t sessionUuid = (lodb_uuid_t)nodeId;
    lodb_.deleteRecord("sessions", sessionUuid);

    LoDbError err = lodb_.insert("sessions", sessionUuid, &session);
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
    meshtastic_LoBBSUser user = meshtastic_LoBBSUser_init_zero;
    LoDbError err = lodb_.get("users", userUuid, &user);
    if (err == LODB_OK)
        return userUuid;
    return 0;
}

bool AuthDal::setUserAdminByUsername(const char *username, bool isAdmin)
{
    meshtastic_LoBBSUser user = meshtastic_LoBBSUser_init_zero;
    if (!loadUserByUsername(username, &user))
        return false;
    user.is_admin = isAdmin;
    lodb_.deleteRecord("users", user.uuid);
    return lodb_.insert("users", user.uuid, &user) == LODB_OK;
}

bool AuthDal::setPasswordByUsername(const char *username, const char *password)
{
    meshtastic_LoBBSUser user = meshtastic_LoBBSUser_init_zero;
    if (!loadUserByUsername(username, &user))
        return false;
    user.password_hash.size = 32;
    hashPassword(password, user.password_hash.bytes);
    lodb_.deleteRecord("users", user.uuid);
    return lodb_.insert("users", user.uuid, &user) == LODB_OK;
}

uint32_t AuthDal::countAdminUsers()
{
    return (uint32_t)lodb_.count("users", [](const void *rec) -> bool {
        const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
        return u->is_admin;
    });
}

uint32_t AuthDal::countAllUsers()
{
    int n = lodb_.count("users");
    return n < 0 ? 0 : (uint32_t)n;
}

bool AuthDal::kickUserByUsername(const char *username)
{
    meshtastic_LoBBSUser user = meshtastic_LoBBSUser_init_zero;
    if (!loadUserByUsername(username, &user))
        return false;
    uint64_t userUuid = user.uuid;

    auto sessions = lodb_.select(
        "sessions",
        [userUuid](const void *rec) -> bool {
            const meshtastic_LoBBSSession *s = (const meshtastic_LoBBSSession *)rec;
            return s->user_uuid == userUuid;
        },
        nullptr);

    for (auto *rec : sessions) {
        const meshtastic_LoBBSSession *s = (const meshtastic_LoBBSSession *)rec;
        lodb_.deleteRecord("sessions", (lodb_uuid_t)s->node_id);
    }
    LoDb::freeRecords(sessions);
    return true;
}

#endif
