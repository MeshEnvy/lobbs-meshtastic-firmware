#if !MESHTASTIC_EXCLUDE_LOBBS

#include "AppUtil.h"
#include "../LoBBSCommandCtx.h"
#include "../LoBBSCommandRegistry.h"
#include "../LoBBSConfig.h"
#include "../LoBBSModule.h"
#include "../LoBBSResponse.h"
#include "Auth/AuthDal.h"
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
    size_t msgLen = strnlen(message, LOBBS_MESSAGE_BODY_MAX);
    if (msgLen <= maxLen) {
        snprintf(buffer, bufferSize, "%.*s", (int)msgLen, message);
    } else {
        size_t copyLen = maxLen < bufferSize - 4 ? maxLen : bufferSize - 4;
        snprintf(buffer, bufferSize, "%.*s...", (int)copyLen, message);
    }
}

bool lobbsAppLoadUser(LoBBSModule *mod, uint64_t uuid, LoScalar *outUser)
{
    if (!mod || !outUser || uuid == 0)
        return false;
    return mod->auth().dal().loadUserByUuid(uuid, outUser);
}

void lobbsAppUsernameForUuid(LoBBSModule *mod, uint64_t uuid, char *buf, size_t bufCap)
{
    if (!buf || bufCap == 0)
        return;
    buf[0] = '\0';
    LoScalar user;
    if (!lobbsAppLoadUser(mod, uuid, &user) || !AuthDal::userUsername(user, buf, bufCap) || !buf[0])
        lobbsAppCopyCapped(buf, bufCap, "unknown", 7);
}

uint64_t lobbsAppUuidForUsername(LoBBSModule *mod, const char *username)
{
    if (!mod || !username || !username[0])
        return 0;
    return mod->auth().dal().getUserUuidByUsername(username);
}

void lobbsAppFormatUint64Decimal(char *buf, size_t bufCap, uint64_t value)
{
    if (!buf || bufCap == 0)
        return;
    char tmp[24];
    int pos = (int)sizeof(tmp);
    tmp[--pos] = '\0';
    if (value == 0) {
        snprintf(buf, bufCap, "0");
        return;
    }
    while (value > 0 && pos > 0) {
        tmp[--pos] = (char)('0' + (value % 10));
        value /= 10;
    }
    strncpy(buf, tmp + pos, bufCap - 1);
    buf[bufCap - 1] = '\0';
}

bool lobbsAppResolveUsername(LoBBSCommandCtx &ctx, const char *username, uint64_t &uuidOut)
{
    uuidOut = lobbsAppUuidForUsername(ctx.mod, username);
    if (uuidOut != 0)
        return true;
    char msg[64];
    snprintf(msg, sizeof(msg), "User '%s' not found.", username ? username : "");
    LoBBSResponse resp;
    lobbsResponseSetError(resp, msg);
    lobbsCommandReplyResponse(ctx, resp);
    return false;
}

#endif
