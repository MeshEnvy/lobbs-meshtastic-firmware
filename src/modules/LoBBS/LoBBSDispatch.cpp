#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDispatch.h"
#include "LoBBSModule.h"
#include "LoBBSPaging.h"
#include "LoBBSVersion.h"
#include "MeshModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "gps/RTC.h"
#include "lobbs.pb.h"
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

static int compareUsernames(const void *a, const void *b)
{
    const meshtastic_LoBBSUser *u1 = (const meshtastic_LoBBSUser *)a;
    const meshtastic_LoBBSUser *u2 = (const meshtastic_LoBBSUser *)b;
    return strcasecmp(u1->username, u2->username);
}

static bool loadUserByUuid(LoBBSDal *dal, uint64_t uuid, meshtastic_LoBBSUser *outUser)
{
    auto users = dal->getDb()->select(
        "users",
        [uuid](const void *rec) -> bool {
            const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
            return u->uuid == uuid;
        },
        nullptr);
    bool found = false;
    if (!users.empty()) {
        *outUser = *(const meshtastic_LoBBSUser *)users[0];
        found = true;
    }
    LoDb::freeRecords(users);
    return found;
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
    return mp.from ? mp.from : nodeDB->getNodeNum();
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

static bool lobbsResolveInboxTarget(LoBBSDal *dal, char *arg1, char *arg2, const meshtastic_LoBBSUser &self, bool isAdmin,
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
    uint64_t uuid = dal->getUserUuidByUsername(arg1);
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

    const uint32_t sessionNodeId = lobbsSessionNodeId(mp);
    LoBBSDal *dal = mod->dal;

    meshtastic_LoBBSUser existingUser = meshtastic_LoBBSUser_init_zero;
    bool isAuthenticated = dal->loadUserByNodeId(sessionNodeId, &existingUser);
    const bool isAdmin = isAuthenticated && existingUser.is_admin;

    if (mp.decoded.payload.size == 0)
        return ProcessMessage::CONTINUE;

    memcpy(mod->msgBuffer, mp.decoded.payload.bytes, mp.decoded.payload.size);
    mod->msgBuffer[mp.decoded.payload.size] = '\0';

    if (mod->msgBuffer[0] != '/') {
        if (isAuthenticated && mod->msgBuffer[0] == '@')
            mod->sendReply(mp, "Use: /mail send <user> <msg>");
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
        lobbsPageClearUser(sessionNodeId);
        mod->sendReply(mp, lobbsHelpLookup(helpTopic, helpVerb, isAuthenticated, isAdmin));
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
        std::string page;
        std::string err;
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
        if (!dal->isValidUsername(username)) {
            mod->sendReply(mp, "Invalid username.");
            return ProcessMessage::CONTINUE;
        }
        char *password = strtok(nullptr, "");
        if (!password || strlen(password) < 5) {
            mod->sendReply(mp, "Usage: /login <username> <password>");
            return ProcessMessage::CONTINUE;
        }
        meshtastic_LoBBSUser dbUser = meshtastic_LoBBSUser_init_zero;
        if (dal->loadUserByUsername(username, &dbUser)) {
            if (!dal->verifyPassword(&dbUser, password)) {
                mod->sendReply(mp, "Invalid password");
                return ProcessMessage::CONTINUE;
            }
            if (dal->loginUser(username, sessionNodeId)) {
                lobbsFormatWelcome(mod->replyBuffer, sizeof(mod->replyBuffer), username, true, dbUser);
                mod->sendReply(mp, mod->replyBuffer);
            } else {
                mod->sendReply(mp, "Error creating session");
            }
        } else {
            if (dal->createUser(username, password, sessionNodeId)) {
                dal->loadUserByUsername(username, &dbUser);
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
        mod->sendReply(mp, lobbsHelpLookup(nullptr, nullptr, false, false));
        return ProcessMessage::CONTINUE;
    }

    if (strcasecmp(cmdName, "/bye") == 0) {
        dal->logoutUser(sessionNodeId);
        mod->sendReply(mp, "Goodbye!");
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
            uint64_t toUuid = dal->getUserUuidByUsername(recipient);
            if (toUuid == 0) {
                mod->sendReply(mp, lobbsUserNotFoundMsg(recipient));
                return ProcessMessage::CONTINUE;
            }
            if (dal->sendMail(existingUser.uuid, toUuid, body2))
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
                inboxUuid = dal->getUserUuidByUsername(targetUser);
                if (inboxUuid == 0) {
                    mod->sendReply(mp, lobbsUserNotFoundMsg(targetUser));
                    return ProcessMessage::CONTINUE;
                }
            }
            auto mailMessages = dal->getAllMailForUser(inboxUuid);
            if (mailMessages.empty()) {
                mod->sendReply(mp, "No mail");
                freeMailMessages(mailMessages);
                return ProcessMessage::CONTINUE;
            }
            int unreadCount = 0;
            for (auto *mailPtr : mailMessages) {
                if (!((const meshtastic_LoBBSMail *)mailPtr)->read)
                    unreadCount++;
            }
            std::string mailList;
            if (unreadCount > 0) {
                char unreadStr[32];
                snprintf(unreadStr, sizeof(unreadStr), "(%d unread)\n", unreadCount);
                mailList += unreadStr;
            }
            for (size_t i = 0; i < mailMessages.size(); i++) {
                const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)mailMessages[i];
                meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
                bool foundSender = loadUserByUuid(dal, mail->from_user_uuid, &sender);
                char entryBuffer[256];
                char timeStr[32];
                char truncMsg[50];
                formatTimeAgo(mail->timestamp, timeStr, sizeof(timeStr));
                truncateMessage(mail->message, truncMsg, sizeof(truncMsg), 25);
                snprintf(entryBuffer, sizeof(entryBuffer), "[%d]%s @%s: %s (%s)\n", (int)(i + 1), mail->read ? "" : "*",
                         foundSender ? sender.username : "unknown", truncMsg, timeStr);
                mailList += entryBuffer;
            }
            mod->sendPagedReply(sessionNodeId, mp, mailList);
            freeMailMessages(mailMessages);
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "read") == 0) {
            char *arg1 = strtok(nullptr, " ");
            char *arg2 = strtok(nullptr, " ");
            uint64_t inboxUuid = 0;
            uint32_t idx = 0;
            std::string err;
            if (!lobbsResolveInboxTarget(dal, arg1, arg2, existingUser, isAdmin, &inboxUuid, &idx, err)) {
                mod->sendReply(mp, err);
                return ProcessMessage::CONTINUE;
            }
            auto mailMessages = dal->getAllMailForUser(inboxUuid);
            if (idx == 0 || idx > mailMessages.size()) {
                mod->sendReply(mp, "Invalid message number");
                freeMailMessages(mailMessages);
                return ProcessMessage::CONTINUE;
            }
            const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)mailMessages[idx - 1];
            meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
            loadUserByUuid(dal, mail->from_user_uuid, &sender);
            char timeStr[32];
            formatTimeAgo(mail->timestamp, timeStr, sizeof(timeStr));
            std::string reply = std::string("From: @") + (sender.username[0] ? sender.username : "unknown") + " (" + timeStr +
                                ")\n" + mail->message;
            mod->sendReply(mp, reply);
            if (inboxUuid == existingUser.uuid)
                dal->markMailAsRead(mail->uuid);
            freeMailMessages(mailMessages);
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "del") == 0) {
            char *arg1 = strtok(nullptr, " ");
            char *arg2 = strtok(nullptr, " ");
            uint64_t inboxUuid = 0;
            uint32_t idx = 0;
            std::string err;
            if (!lobbsResolveInboxTarget(dal, arg1, arg2, existingUser, isAdmin, &inboxUuid, &idx, err)) {
                mod->sendReply(mp, err);
                return ProcessMessage::CONTINUE;
            }
            if (dal->deleteMailInboxIndex(inboxUuid, idx))
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
            auto newsItems = dal->getAllNewsForUser(existingUser.uuid);
            if (newsItems.empty()) {
                mod->sendReply(mp, "No news");
                freeNewsEntries(newsItems);
                return ProcessMessage::CONTINUE;
            }
            int unreadCount = 0;
            for (const auto &entry : newsItems) {
                if (!entry.isRead)
                    unreadCount++;
            }
            std::string newsList;
            if (unreadCount > 0) {
                char unreadStr[32];
                snprintf(unreadStr, sizeof(unreadStr), "(%d unread)\n", unreadCount);
                newsList += unreadStr;
            }
            for (size_t i = 0; i < newsItems.size(); i++) {
                const meshtastic_LoBBSNews *news = newsItems[i].news;
                meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
                loadUserByUuid(dal, news->author_user_uuid, &author);
                char entryBuffer[256];
                char timeStr[32];
                char truncMsg[50];
                formatTimeAgo(news->timestamp, timeStr, sizeof(timeStr));
                truncateMessage(news->message, truncMsg, sizeof(truncMsg), 25);
                snprintf(entryBuffer, sizeof(entryBuffer), "[%d]%s @%s: %s (%s)\n", (int)(i + 1),
                         newsItems[i].isRead ? "" : "*", author.username[0] ? author.username : "unknown", truncMsg, timeStr);
                newsList += entryBuffer;
            }
            mod->sendPagedReply(sessionNodeId, mp, newsList);
            freeNewsEntries(newsItems);
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "read") == 0) {
            char *arg1 = strtok(nullptr, " ");
            if (!arg1 || !lobbsIsDigitsOnly(arg1)) {
                mod->sendReply(mp, "Usage: /news read <n>");
                return ProcessMessage::CONTINUE;
            }
            uint32_t idx = (uint32_t)atoi(arg1);
            auto newsItems = dal->getAllNewsForUser(existingUser.uuid);
            if (idx == 0 || idx > newsItems.size()) {
                mod->sendReply(mp, "Invalid news number");
                freeNewsEntries(newsItems);
                return ProcessMessage::CONTINUE;
            }
            const meshtastic_LoBBSNews *news = newsItems[idx - 1].news;
            meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
            loadUserByUuid(dal, news->author_user_uuid, &author);
            char timeStr[32];
            formatTimeAgo(news->timestamp, timeStr, sizeof(timeStr));
            std::string reply = std::string("From: @") + (author.username[0] ? author.username : "unknown") + " (" + timeStr +
                                ")\n" + news->message;
            mod->sendReply(mp, reply);
            dal->markNewsAsRead(news->uuid, existingUser.uuid);
            freeNewsEntries(newsItems);
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "post") == 0) {
            const char *body = lobbsPayloadAfterPrefix(mp, "/news post ");
            if (!body || !isalpha((unsigned char)*body)) {
                mod->sendReply(mp, "Usage: /news post <msg>");
                return ProcessMessage::CONTINUE;
            }
            if (dal->postNews(existingUser.uuid, body))
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
            auto newsItems = dal->getAllNewsForUser(existingUser.uuid);
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
            if (dal->deleteNewsUuid(newsUuid))
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
            if (filterStr && !dal->isValidUsername(filterStr)) {
                mod->sendReply(mp, "Invalid filter.");
                return ProcessMessage::CONTINUE;
            }
            auto username_filter = [filterStr](const void *rec) -> bool {
                const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
                return !filterStr || !filterStr[0] || stristr(u->username, filterStr) != nullptr;
            };
            auto users = dal->getDb()->select("users", username_filter, compareUsernames);
            if (users.empty()) {
                mod->sendReply(mp, filterStr && filterStr[0] ? "No users match filter." : "No users found");
                LoDb::freeRecords(users);
                return ProcessMessage::CONTINUE;
            }
            std::string userListMsg = "Users:\n";
            for (size_t i = 0; i < users.size(); i++) {
                const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)users[i];
                if (i > 0)
                    userListMsg += ", ";
                userListMsg += u->username;
                if (u->is_admin)
                    userListMsg += "*";
            }
            mod->sendPagedReply(sessionNodeId, mp, userListMsg);
            LoDb::freeRecords(users);
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
            if (dal->kickUserByUsername(target))
                mod->sendReply(mp, "Sessions cleared.");
            else
                mod->sendReply(mp, lobbsUserNotFoundMsg(target));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "promote") == 0) {
            if (dal->setUserAdminByUsername(target, true))
                mod->sendReply(mp, "Promoted.");
            else
                mod->sendReply(mp, lobbsUserNotFoundMsg(target));
            return ProcessMessage::CONTINUE;
        }
        if (strcasecmp(verb, "demote") == 0) {
            meshtastic_LoBBSUser targetUser = meshtastic_LoBBSUser_init_zero;
            if (!dal->loadUserByUsername(target, &targetUser)) {
                mod->sendReply(mp, lobbsUserNotFoundMsg(target));
                return ProcessMessage::CONTINUE;
            }
            if (targetUser.is_admin && dal->countAdminUsers() <= 1) {
                mod->sendReply(mp, "Cannot demote last admin.");
                return ProcessMessage::CONTINUE;
            }
            if (dal->setUserAdminByUsername(target, false))
                mod->sendReply(mp, "Demoted.");
            else
                mod->sendReply(mp, "Failed.");
            return ProcessMessage::CONTINUE;
        }
        mod->sendReply(mp, "Try /help user");
        return ProcessMessage::CONTINUE;
    }

    mod->sendReply(mp, lobbsHelpLookup(nullptr, nullptr, true, isAdmin));
    return ProcessMessage::CONTINUE;
}

#endif
