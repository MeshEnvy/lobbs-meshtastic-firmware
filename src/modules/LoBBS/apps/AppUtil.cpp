#if !MESHTASTIC_EXCLUDE_LOBBS

#include "AppUtil.h"
#include "gps/RTC.h"
#include <cstdio>
#include <cstring>

void lobbsAppCopyCapped(char *dst, size_t dstCap, const char *src, size_t srcCap)
{
    size_t n = 0;
    if (dst && dstCap > 0) {
        if (src) {
            while (n + 1 < dstCap && n < srcCap && src[n] != '\0')
                n++;
            memcpy(dst, src, n);
        }
        dst[n] = '\0';
    }
}

void lobbsAppTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize)
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

void lobbsAppTruncMsg(const char *message, char *buffer, size_t bufferSize, size_t maxLen)
{
    if (!message)
        message = "";
    size_t msgLen = strnlen(message, 200);
    if (msgLen <= maxLen) {
        snprintf(buffer, bufferSize, "%.*s", (int)msgLen, message);
    } else {
        size_t copyLen = maxLen < bufferSize - 4 ? maxLen : bufferSize - 4;
        snprintf(buffer, bufferSize, "%.*s...", (int)copyLen, message);
    }
}

bool lobbsAppLoadUser(LoBBSDal *dal, uint64_t uuid, meshtastic_LoBBSUser *outUser)
{
    auto users = dal->getDb()->select(
        "users",
        [uuid](const void *rec) -> bool { return ((const meshtastic_LoBBSUser *)rec)->uuid == uuid; }, nullptr);
    bool found = !users.empty();
    if (found)
        *outUser = *(const meshtastic_LoBBSUser *)users[0];
    LoDb::freeRecords(users);
    return found;
}

void lobbsAppDrawTitled(LobbsHistory *h, const LobbsFrame *self, const char *title)
{
    char buf[200];
    size_t n = 0;
    int w = snprintf(buf, sizeof(buf), "%s\n", title);
    if (w > 0)
        n = (size_t)w;
    for (uint8_t i = 0; i < self->itemCount && n + 1 < sizeof(buf); i++) {
        w = snprintf(buf + n, sizeof(buf) - n, "[%u] %s\n", (unsigned)(i + 1), self->items[i].label);
        if (w < 0)
            break;
        n += (size_t)w;
    }
    snprintf(buf + n, sizeof(buf) - n, "? < << p");
    lobbsHistoryReply(h, buf);
}

void lobbsAppDrawPrompt(LobbsHistory *h, const LobbsFrame *self)
{
    lobbsHistoryReply(h, self->prompt ? self->prompt : "?");
}

#endif
