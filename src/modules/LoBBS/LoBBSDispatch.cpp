#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDispatch.h"
#include "LoBBSModule.h"
#include "LoBBSMenu.h"
#include "apps/Mail/MailDal.h"
#include "apps/News/NewsDal.h"
#include "apps/Auth/AuthDal.h"
#include "LoBBSPaging.h"
#include "LoBBSVersion.h"
#include "MeshModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "gps/RTC.h"
#include "apps/Auth/auth.pb.h"
#include "mesh/NodeDB.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

static std::string lobbsUserNotFoundMsg(const char *username)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "User '%s' not found.", username ? username : "?");
    return buf;
}

static const char *stristr(const char *haystack, const char *needle)
{
    if (!*needle)
        return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower(*h) == tolower(*n)) {
            h++;
            n++;
        }
        if (!*n)
            return haystack;
    }
    return nullptr;
}

static void formatTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize)
{
    uint32_t now = getTime();
    if (now < timestamp) {
        snprintf(buffer, bufferSize, "now");
        return;
    }
    uint32_t diff = now - timestamp;
    if (diff < 60)
        snprintf(buffer, bufferSize, "%ds ago", diff);
    else if (diff < 3600)
        snprintf(buffer, bufferSize, "%dm ago", diff / 60);
    else if (diff < 86400)
        snprintf(buffer, bufferSize, "%dh ago", diff / 3600);
    else
        snprintf(buffer, bufferSize, "%dd ago", diff / 86400);
}

static void truncateMessage(const char *message, char *buffer, size_t bufferSize, size_t maxLen)
{
    size_t msgLen = strlen(message);
    if (msgLen <= maxLen) {
        strncpy(buffer, message, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    } else {
        size_t copyLen = maxLen < bufferSize - 4 ? maxLen : bufferSize - 4;
        strncpy(buffer, message, copyLen);
        buffer[copyLen] = '\0';
        strncat(buffer, "...", bufferSize - strlen(buffer) - 1);
    }
}

static void freeMailMessages(std::vector<void *> &mailMessages)
{
    LoDb::freeRecords(mailMessages);
}

static void freeNewsEntries(std::vector<LoBBSNewsEntry> &newsItems)
{
    for (auto &entry : newsItems) {
        delete[] (uint8_t *)entry.news;
    }
    newsItems.clear();
}

static uint32_t lobbsSessionNodeId(const meshtastic_MeshPacket &mp)
{
    return getFrom(&mp);
}

static void lobbsTrimLine(char *line)
{
    if (!line)
        return;
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' || line[len - 1] == '\r' || line[len - 1] == '\n'))
        line[--len] = '\0';
    char *start = line;
    while (*start == ' ' || *start == '\t')
        start++;
    if (start != line)
        memmove(line, start, strlen(start) + 1);
}

// Phone clients sometimes loop our DM replies back with from=0; do not answer those.
static bool lobbsLooksLikeEchoedReply(const char *text, size_t len)
{
    if (!text || len == 0)
        return false;
    if (len >= 7 && strncmp(text, "LoBBS v", 7) == 0)
        return true;
    if (len >= 5 && strncmp(text, "Mail\n", 5) == 0)
        return true;
    if (len >= 5 && strncmp(text, "News\n", 5) == 0)
        return true;
    if (len >= 6 && strncmp(text, "Users\n", 6) == 0)
        return true;
    if (strstr(text, "\n[1]") && strstr(text, "? < << p"))
        return true;
    if (strncmp(text, "From: @", 7) == 0)
        return true;
    if (len >= 4 && text[0] == '[' && isdigit((unsigned char)text[1]) && text[2] == ']' &&
        (text[3] == ' ' || text[3] == '*'))
        return true;
    if (strcmp(text, "Number?") == 0 || strcmp(text, "Delete #?") == 0 || strcmp(text, "No mail") == 0)
        return true;
    return false;
}

static bool lobbsIgnoreLoopbackPacket(const meshtastic_MeshPacket &mp, const char *text, size_t len)
{
    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.from == ourNode)
        return true;
    if (isFromUs(&mp) && lobbsLooksLikeEchoedReply(text, len))
        return true;
    return false;
}

static bool lobbsIsDigitsOnly(const char *s)
{
    if (!s || !*s)
        return false;
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p))
            return false;
    }
    return true;
}

static void lobbsFormatWelcome(char *buf, size_t bufSize, const char *username, bool returning,
                               const meshtastic_LoBBSUser &user)
{
    if (returning)
        snprintf(buf, bufSize, "Welcome back %s!", username);
    else
        snprintf(buf, bufSize, "Welcome %s!", username);
    if (user.is_admin) {
        size_t len = strlen(buf);
        snprintf(buf + len, bufSize - len, "\nYou are the admin.");
    }
}

static void lobbsFormatWhoami(char *buf, size_t bufSize, const meshtastic_LoBBSUser &user)
{
    snprintf(buf, bufSize, "Logged in as %s.", user.username);
    if (user.is_admin) {
        size_t len = strlen(buf);
        snprintf(buf + len, bufSize - len, "\nYou are the admin.");
    }
}

static void lobbsRemoveToken(char *line, const char *token)
{
    char *p = line;
    while ((p = const_cast<char *>(stristr(p, token))) != nullptr) {
        size_t tlen = strlen(token);
        memmove(p, p + tlen, strlen(p + tlen) + 1);
        while (p > line && (p[-1] == ' ' || p[-1] == '\t'))
            memmove(p - 1, p, strlen(p) + 1);
    }
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
        line[--len] = '\0';
}

static std::string lobbsHelpLookup(const char *topic, const char *verb, bool isAuth, bool isAdmin)
{
    if (!topic || !topic[0]) {
        if (isAuth)
            return std::string("LoBBS v") + LOBBS_VERSION_SHORT +
                   "\n/mail /news /user then a verb\nExample: /mail list\n/whoami /bye /help /p";
        return std::string("LoBBS v") + LOBBS_VERSION_SHORT + "\n/help /login /whoami";
    }
    if (strcasecmp(topic, "p") == 0)
        return "Usage: /p or /p <n>\nNext page or page n of last list.";
    if (strcasecmp(topic, "login") == 0)
        return "Usage: /login <user> <pass>\nLogin or create account.";
    if (strcasecmp(topic, "mail") == 0) {
        if (!verb || !verb[0]) {
            std::string h = "Mail: send list read del\n/mail send <u> <msg>";
            if (isAdmin)
                h += "\nAdmin: list/read/del <user> ...";
            return h;
        }
        if (strcasecmp(verb, "send") == 0)
            return "Usage: /mail send <user> <msg>\nRequires login.";
        if (strcasecmp(verb, "list") == 0)
            return isAdmin ? "Usage: /mail list or /mail list <user>\nRequires login."
                           : "Usage: /mail list\nYour inbox. Requires login.";
        if (strcasecmp(verb, "read") == 0)
            return isAdmin ? "Usage: /mail read <n> or /mail read <user> <n>"
                           : "Usage: /mail read <n>\nRequires login.";
        if (strcasecmp(verb, "del") == 0)
            return isAdmin ? "Usage: /mail del <n> or /mail del <user> <n>" : "Usage: /mail del <n>\nRequires login.";
        return "Try /help mail";
    }
    if (strcasecmp(topic, "news") == 0) {
        if (!verb || !verb[0])
            return "News: list read post del\n/news list | read <n> | post <msg> | del <n>";
        if (strcasecmp(verb, "list") == 0)
            return "Usage: /news list\nRequires login.";
        if (strcasecmp(verb, "read") == 0)
            return "Usage: /news read <n>\nRequires login.";
        if (strcasecmp(verb, "post") == 0)
            return "Usage: /news post <msg>\nRequires login.";
        if (strcasecmp(verb, "del") == 0)
            return "Usage: /news del <n>\nAuthor or admin.";
        return "Try /help news";
    }
    if (strcasecmp(topic, "user") == 0) {
        if (!verb || !verb[0]) {
            std::string h = "User: list\n/user list [filter]";
            if (isAdmin)
                h += "\nAdmin: kick promote demote <user>";
            return h;
        }
        if (strcasecmp(verb, "list") == 0)
            return "Usage: /user list [filter]\nRequires login.";
        if (strcasecmp(verb, "kick") == 0)
            return isAdmin ? "Usage: /user kick <user>\nAdmin only." : "Admin only.";
        if (strcasecmp(verb, "promote") == 0)
            return isAdmin ? "Usage: /user promote <user>\nAdmin only." : "Admin only.";
        if (strcasecmp(verb, "demote") == 0)
            return isAdmin ? "Usage: /user demote <user>\nAdmin only." : "Admin only.";
        return "Try /help user";
    }
    return "Unknown. Try /help";
}

static const char *lobbsPayloadAfterPrefix(const meshtastic_MeshPacket &mp, const char *prefix)
{
    const char *msgStart = (const char *)mp.decoded.payload.bytes;
    size_t payloadSize = mp.decoded.payload.size;
    const char *payloadEnd = msgStart + payloadSize;
    size_t prefixLen = strlen(prefix);
    if (payloadSize < prefixLen || strncasecmp(msgStart, prefix, prefixLen) != 0)
        return nullptr;
    msgStart += prefixLen;
    while (msgStart < payloadEnd && *msgStart == ' ')
        msgStart++;
    if (msgStart >= payloadEnd || *msgStart == '\0')
        return nullptr;
    return msgStart;
}

static bool lobbsResolveInboxTarget(LoBBSModule *mod, char *arg1, char *arg2, const meshtastic_LoBBSUser &self, bool isAdmin,
                                    uint64_t *inboxUuid, uint32_t *indexOut, std::string &err)
{
    if (!arg1) {
        err = "Missing index.";
        return false;
    }
    if (lobbsIsDigitsOnly(arg1)) {
        *inboxUuid = self.uuid;
        *indexOut = (uint32_t)atoi(arg1);
        return true;
    }
    if (!isAdmin) {
        err = "Admin only.";
        return false;
    }
    if (!arg2 || !lobbsIsDigitsOnly(arg2)) {
        err = "Usage: <user> <n>";
        return false;
    }
    uint64_t uuid = mod->auth().dal().getUserUuidByUsername(arg1);
    if (uuid == 0) {
        err = lobbsUserNotFoundMsg(arg1);
        return false;
    }
    *inboxUuid = uuid;
    *indexOut = (uint32_t)atoi(arg2);
    return true;
}

ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp)
{
    if (!isToUs(&mp))
        return ProcessMessage::CONTINUE;

    if (mp.decoded.payload.size == 0)
        return ProcessMessage::CONTINUE;

    size_t copyLen = mp.decoded.payload.size;
    if (copyLen >= sizeof(mod->msgBuffer))
        copyLen = sizeof(mod->msgBuffer) - 1;
    memcpy(mod->msgBuffer, mp.decoded.payload.bytes, copyLen);
    mod->msgBuffer[copyLen] = '\0';

    if (lobbsIgnoreLoopbackPacket(mp, mod->msgBuffer, mp.decoded.payload.size))
        return ProcessMessage::CONTINUE;

    const uint32_t sessionNodeId = lobbsSessionNodeId(mp);
    AuthDal &auth = mod->auth().dal();

    meshtastic_LoBBSUser existingUser = meshtastic_LoBBSUser_init_zero;
    bool isAuthenticated = auth.loadUserByNodeId(sessionNodeId, &existingUser);
    const bool isAdmin = isAuthenticated && existingUser.is_admin;

    lobbsTrimLine(mod->msgBuffer);
    if (mod->msgBuffer[0] == '\0')
        return ProcessMessage::CONTINUE;

    if (mod->msgBuffer[0] != '/') {
        const char *line = mod->msgBuffer;
        if (lobbsMenuTryGlobalKeys(mod, mp, sessionNodeId, isAuthenticated, isAuthenticated ? &existingUser : nullptr,
                                   isAdmin, line) == LobbsMenuKeyResult::Handled)
            return ProcessMessage::CONTINUE;
        lobbsMenuHandleLine(mod, mp, sessionNodeId, isAuthenticated, isAuthenticated ? &existingUser : nullptr, isAdmin,
                            line);
        return ProcessMessage::CONTINUE;
    }

    char lineCopy[256];
    strncpy(lineCopy, mod->msgBuffer, sizeof(lineCopy) - 1);
    lineCopy[sizeof(lineCopy) - 1] = '\0';
    bool wantHelp = stristr(lineCopy, "--help") != nullptr;
    lobbsRemoveToken(lineCopy, "--help");

    char *cmdName = strtok(lineCopy, " ");
    if (!cmdName || cmdName[0] != '/' || cmdName[1] == '\0')
        return ProcessMessage::CONTINUE;

    const char *helpTopic = nullptr;
    const char *helpVerb = nullptr;
    if (strcasecmp(cmdName, "/help") == 0 || strcasecmp(cmdName, "/hi") == 0) {
        wantHelp = true;
        helpTopic = strtok(nullptr, " ");
        helpVerb = strtok(nullptr, " ");
    } else if (wantHelp) {
        helpTopic = cmdName + 1;
        helpVerb = strtok(nullptr, " ");
    }

    if (wantHelp) {
        lobbsMenuReprint(mod, mp, sessionNodeId, isAuthenticated, isAuthenticated ? &existingUser : nullptr, isAdmin);
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/p") == 0) {
        char *pageArg = strtok(nullptr, " ");
        int pageNum = 0;
        if (pageArg) {
            if (!lobbsIsDigitsOnly(pageArg)) {
                mod->sendReply(mp, "Usage: /p or /p <n>");
                return ProcessMessage::CONTINUE;
            }
            pageNum = atoi(pageArg);
        }
        const char *page = nullptr;
        const char *err = nullptr;
        if (!lobbsPageFetch(sessionNodeId, pageNum, page, err))
            mod->sendReply(mp, err);
        else
            mod->sendReply(mp, page);
        return ProcessMessage::CONTINUE;
    }

    lobbsPageClearUser(sessionNodeId);

    if (strcasecmp(cmdName, "/login") == 0) {
        char *username = strtok(nullptr, " ");
        if (!username) {
            mod->sendReply(mp, "Usage: /login <username> <password>");
            return ProcessMessage::CONTINUE;
        }
        if (!auth.isValidUsername(username)) {
            mod->sendReply(mp, "Invalid username.");
            return ProcessMessage::CONTINUE;
        }
        char *password = strtok(nullptr, "");
        if (!password || strlen(password) < 5) {
            mod->sendReply(mp, "Usage: /login <username> <password>");
            return ProcessMessage::CONTINUE;
        }
        meshtastic_LoBBSUser dbUser = meshtastic_LoBBSUser_init_zero;
        if (auth.loadUserByUsername(username, &dbUser)) {
            if (!auth.verifyPassword(&dbUser, password)) {
                mod->sendReply(mp, "Invalid password");
                return ProcessMessage::CONTINUE;
            }
            if (auth.loginUser(username, sessionNodeId)) {
                lobbsFormatWelcome(mod->replyBuffer, sizeof(mod->replyBuffer), username, true, dbUser);
                mod->sendReply(mp, mod->replyBuffer);
            } else {
                mod->sendReply(mp, "Error creating session");
            }
        } else {
            if (auth.createUser(username, password, sessionNodeId)) {
                auth.loadUserByUsername(username, &dbUser);
                lobbsFormatWelcome(mod->replyBuffer, sizeof(mod->replyBuffer), username, false, dbUser);
                mod->sendReply(mp, mod->replyBuffer);
            } else {
                mod->sendReply(mp, "Error creating account");
            }
        }
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/whoami") == 0) {
        if (!isAuthenticated)
            mod->sendReply(mp, "Not logged in.");
        else {
            lobbsFormatWhoami(mod->replyBuffer, sizeof(mod->replyBuffer), existingUser);
            mod->sendReply(mp, mod->replyBuffer);
        }
        return ProcessMessage::CONTINUE;
    }

    if (!isAuthenticated) {
        lobbsMenuReprint(mod, mp, sessionNodeId, false, nullptr, false);
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/bye") == 0) {
        auth.logoutUser(sessionNodeId);
        lobbsMenuOnLogout(sessionNodeId);
        lobbsPageClearUser(sessionNodeId);
        mod->sendReply(mp, "Goodbye!");
        mod->sendReply(mp, std::string("LoBBS v") + LOBBS_VERSION_SHORT + "\n[1] Login\n[2] Who am I" + "\n? < << p");
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/users") == 0) {
        mod->sendReply(mp, "Use: /user list");
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/mail") == 0) {
        char *verb = strtok(nullptr, " ");
        if (!verb) {
            mod->sendReply(mp, lobbsHelpLookup("mail", nullptr, true, isAdmin));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "send") == 0) {
            char *recipient = strtok(nullptr, " ");
            if (!recipient) {
                mod->sendReply(mp, "Usage: /mail send <user> <msg>");
                return ProcessMessage::CONTINUE;
            }
            char sendPrefix[128];
            snprintf(sendPrefix, sizeof(sendPrefix), "/mail send %s ", recipient);
            const char *body2 = lobbsPayloadAfterPrefix(mp, sendPrefix);
            if (!body2) {
                mod->sendReply(mp, "Message empty.");
                return ProcessMessage::CONTINUE;
            }
            uint64_t toUuid = auth.getUserUuidByUsername(recipient);
            if (toUuid == 0) {
                mod->sendReply(mp, lobbsUserNotFoundMsg(recipient));
                return ProcessMessage::CONTINUE;
            }
            if (mod->mail().dal().sendMail(existingUser.uuid, toUuid, body2))
                mod->sendReply(mp, "Mail sent.");
            else
                mod->sendReply(mp, "Failed to send mail.");
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "list") == 0) {
            char *targetUser = strtok(nullptr, " ");
            uint64_t inboxUuid = existingUser.uuid;
            if (targetUser) {
                if (!isAdmin) {
                    mod->sendReply(mp, "Admin only.");
                    return ProcessMessage::CONTINUE;
                }
                inboxUuid = auth.getUserUuidByUsername(targetUser);
                if (inboxUuid == 0) {
                    mod->sendReply(mp, lobbsUserNotFoundMsg(targetUser));
                    return ProcessMessage::CONTINUE;
                }
            }
            lobbsMenuShowMailList(mod, mp, sessionNodeId, &existingUser, isAdmin, inboxUuid);
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "read") == 0) {
            char *arg1 = strtok(nullptr, " ");
            char *arg2 = strtok(nullptr, " ");
            uint64_t inboxUuid = 0;
            uint32_t idx = 0;
            std::string err;
            if (!lobbsResolveInboxTarget(mod, arg1, arg2, existingUser, isAdmin, &inboxUuid, &idx, err)) {
                mod->sendReply(mp, err);
                return ProcessMessage::CONTINUE;
            }
            lobbsMenuShowMailRead(mod, mp, sessionNodeId, &existingUser, isAdmin, inboxUuid, idx);
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "del") == 0) {
            char *arg1 = strtok(nullptr, " ");
            char *arg2 = strtok(nullptr, " ");
            uint64_t inboxUuid = 0;
            uint32_t idx = 0;
            std::string err;
            if (!lobbsResolveInboxTarget(mod, arg1, arg2, existingUser, isAdmin, &inboxUuid, &idx, err)) {
                mod->sendReply(mp, err);
                return ProcessMessage::CONTINUE;
            }
            if (mod->mail().dal().deleteMailInboxIndex(inboxUuid, idx))
                mod->sendReply(mp, "Deleted.");
            else
                mod->sendReply(mp, "Invalid message number");
            return ProcessMessage::CONTINUE;
        }
        mod->sendReply(mp, "Try /help mail");
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/news") == 0) {
        char *verb = strtok(nullptr, " ");
        if (!verb) {
            mod->sendReply(mp, lobbsHelpLookup("news", nullptr, true, isAdmin));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "list") == 0) {
            lobbsMenuShowNewsList(mod, mp, sessionNodeId, &existingUser, isAdmin);
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "read") == 0) {
            char *arg1 = strtok(nullptr, " ");
            if (!arg1 || !lobbsIsDigitsOnly(arg1)) {
                mod->sendReply(mp, "Usage: /news read <n>");
                return ProcessMessage::CONTINUE;
            }
            lobbsMenuShowNewsRead(mod, mp, sessionNodeId, &existingUser, isAdmin, (uint32_t)atoi(arg1));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "post") == 0) {
            const char *body = lobbsPayloadAfterPrefix(mp, "/news post ");
            if (!body || !isalpha((unsigned char)*body)) {
                mod->sendReply(mp, "Usage: /news post <msg>");
                return ProcessMessage::CONTINUE;
            }
            if (mod->news().dal().postNews(existingUser.uuid, body))
                mod->sendReply(mp, "News posted");
            else
                mod->sendReply(mp, "Failed to post news");
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "del") == 0) {
            char *arg1 = strtok(nullptr, " ");
            if (!arg1 || !lobbsIsDigitsOnly(arg1)) {
                mod->sendReply(mp, "Usage: /news del <n>");
                return ProcessMessage::CONTINUE;
            }
            uint32_t idx = (uint32_t)atoi(arg1);
            auto newsItems = mod->news().dal().getAllNewsForUser(existingUser.uuid);
            if (idx == 0 || idx > newsItems.size()) {
                mod->sendReply(mp, "Invalid news number");
                freeNewsEntries(newsItems);
                return ProcessMessage::CONTINUE;
            }
            const meshtastic_LoBBSNews *news = newsItems[idx - 1].news;
            bool allowed = isAdmin || news->author_user_uuid == existingUser.uuid;
            uint64_t newsUuid = news->uuid;
            freeNewsEntries(newsItems);
            if (!allowed) {
                mod->sendReply(mp, "Not allowed.");
                return ProcessMessage::CONTINUE;
            }
            if (mod->news().dal().deleteNewsUuid(newsUuid))
                mod->sendReply(mp, "Deleted.");
            else
                mod->sendReply(mp, "Failed to delete.");
            return ProcessMessage::CONTINUE;
        }
        mod->sendReply(mp, "Try /help news");
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/user") == 0) {
        char *verb = strtok(nullptr, " ");
        if (!verb) {
            mod->sendReply(mp, lobbsHelpLookup("user", nullptr, true, isAdmin));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "list") == 0) {
            char *filterStr = strtok(nullptr, " ");
            if (filterStr && !auth.isValidUsername(filterStr)) {
                mod->sendReply(mp, "Invalid filter.");
                return ProcessMessage::CONTINUE;
            }
            std::string userListMsg;
            const char *emptyReply = nullptr;
            if (!auth.buildUserList(filterStr, userListMsg, &emptyReply)) {
                mod->sendReply(mp, emptyReply);
                return ProcessMessage::CONTINUE;
            }
            mod->sendPagedReply(sessionNodeId, mp, userListMsg.c_str());
            lobbsMenuAfterUserList(sessionNodeId);
            return ProcessMessage::CONTINUE;
        }
        if (!isAdmin) {
            mod->sendReply(mp, "Admin only.");
            return ProcessMessage::CONTINUE;
        }
        char *target = strtok(nullptr, " ");
        if (!target) {
            mod->sendReply(mp, lobbsHelpLookup("user", verb, true, true));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "kick") == 0) {
            if (auth.kickUserByUsername(target))
                mod->sendReply(mp, "Sessions cleared.");
            else
                mod->sendReply(mp, lobbsUserNotFoundMsg(target));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "promote") == 0) {
            if (auth.setUserAdminByUsername(target, true))
                mod->sendReply(mp, "Promoted.");
            else
                mod->sendReply(mp, lobbsUserNotFoundMsg(target));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "demote") == 0) {
            meshtastic_LoBBSUser targetUser = meshtastic_LoBBSUser_init_zero;
            if (!auth.loadUserByUsername(target, &targetUser)) {
                mod->sendReply(mp, lobbsUserNotFoundMsg(target));
                return ProcessMessage::CONTINUE;
            }
            if (targetUser.is_admin && auth.countAdminUsers() <= 1) {
                mod->sendReply(mp, "Cannot demote last admin.");
                return ProcessMessage::CONTINUE;
            }
            if (auth.setUserAdminByUsername(target, false))
                mod->sendReply(mp, "Demoted.");
            else
                mod->sendReply(mp, "Failed.");
            return ProcessMessage::CONTINUE;
        }
        mod->sendReply(mp, "Try /help user");
        return ProcessMessage::CONTINUE;
    }

    lobbsMenuReprint(mod, mp, sessionNodeId, isAuthenticated, &existingUser, isAdmin);
    return ProcessMessage::CONTINUE;
}

#endif
