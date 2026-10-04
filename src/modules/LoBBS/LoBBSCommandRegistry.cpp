#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSCommandRegistry.h"
#include "LoBBSHooks.h"
#include "LoBBSModule.h"
#include "LoBBSReply.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void lobbsSkipWs(char *&p)
{
    while (p && (*p == ' ' || *p == '\t'))
        p++;
}

static bool lobbsParsePageToken(const char *tok, uint32_t &pageOut)
{
    if (!tok || (tok[0] != 'p' && tok[0] != 'P') || !tok[1])
        return false;
    for (size_t i = 1; tok[i]; i++) {
        if (!isdigit((unsigned char)tok[i]))
            return false;
    }
    pageOut = (uint32_t)atoi(tok + 1);
    if (pageOut == 0)
        pageOut = 1;
    return true;
}

void lobbsCommandReply(LoBBSCommandCtx &ctx, const char *body)
{
    if (!body)
        body = "";
    if (!ctx.mod || !ctx.mp)
        return;
    if (ctx.reqId == 0) {
        ctx.mod->sendReply(*ctx.mp, body);
        return;
    }
    char buf[LOBBS_REPLY_BYTES + 32];
    snprintf(buf, sizeof(buf), "<%u>%s", ctx.reqId, body);
    ctx.mod->sendReply(*ctx.mp, buf);
}

const char *lobbsArgShift(LoBBSCommandCtx &ctx)
{
    if (!ctx.rest)
        return nullptr;
    lobbsSkipWs(ctx.rest);
    if (!ctx.rest[0])
        return nullptr;
    char *start = ctx.rest;
    while (*ctx.rest && *ctx.rest != ' ' && *ctx.rest != '\t')
        ctx.rest++;
    if (*ctx.rest) {
        *ctx.rest++ = '\0';
        lobbsSkipWs(ctx.rest);
    }
    return start;
}

const char *lobbsArgPeek(const LoBBSCommandCtx &ctx)
{
    if (!ctx.rest)
        return nullptr;
    char *p = ctx.rest;
    lobbsSkipWs(p);
    return p[0] ? p : nullptr;
}

bool lobbsArgTakePage(LoBBSCommandCtx &ctx)
{
    if (!ctx.rest)
        return false;
    lobbsSkipWs(ctx.rest);
    if (!ctx.rest[0])
        return false;
    char *start = ctx.rest;
    char *end = start;
    while (*end && *end != ' ' && *end != '\t')
        end++;
    char saved = *end;
    *end = '\0';
    uint32_t page = 1;
    bool ok = lobbsParsePageToken(start, page);
    *end = saved;
    if (!ok)
        return false;
    ctx.page = page;
    ctx.rest = end;
    if (*ctx.rest)
        ctx.rest++;
    lobbsSkipWs(ctx.rest);
    return true;
}

const char *lobbsArgRest(LoBBSCommandCtx &ctx)
{
    if (!ctx.rest)
        return "";
    lobbsSkipWs(ctx.rest);
    return ctx.rest;
}

bool lobbsArgHasMore(const LoBBSCommandCtx &ctx)
{
    return lobbsArgPeek(ctx) != nullptr;
}

bool lobbsTokenIsPage(const char *tok)
{
    uint32_t page = 1;
    return lobbsParsePageToken(tok, page);
}

bool lobbsTokIsUint(const char *tok, uint32_t &out)
{
    if (!tok || !tok[0])
        return false;
    char *end = nullptr;
    unsigned long v = strtoul(tok, &end, 10);
    if (end == tok || (end && *end != '\0'))
        return false;
    if (v > UINT32_MAX)
        return false;
    out = (uint32_t)v;
    return true;
}

bool lobbsArgPeekIsUint(const LoBBSCommandCtx &ctx)
{
    uint32_t dummy = 0;
    return lobbsArgPeekUint(ctx, dummy);
}

bool lobbsArgPeekUint(const LoBBSCommandCtx &ctx, uint32_t &out)
{
    return lobbsTokIsUint(lobbsArgPeek(ctx), out);
}

bool lobbsArgShiftUint(LoBBSCommandCtx &ctx, uint32_t &out)
{
    const char *tok = lobbsArgShift(ctx);
    return lobbsTokIsUint(tok, out);
}

int lobbsArgShiftMany(LoBBSCommandCtx &ctx, const char *out[], int maxOut)
{
    if (!out || maxOut <= 0)
        return 0;
    int n = 0;
    while (n < maxOut) {
        const char *t = lobbsArgShift(ctx);
        if (!t)
            break;
        out[n++] = t;
    }
    return n;
}

bool lobbsHelpQueryMatches(const char *query, const char *prefix)
{
    if (!query || !prefix || !prefix[0])
        return false;
    if (strcasecmp(query, prefix) == 0)
        return true;
    size_t plen = strlen(prefix);
    if (strncasecmp(query, prefix, plen) != 0)
        return false;
    return query[plen] == ' ';
}

void lobbsRootCommandPush(std::vector<std::string> &lines, const char *name)
{
    if (name && name[0])
        lines.push_back(name);
}

void lobbsCommandHelpPush(std::vector<std::string> &lines, const char *topic, const LoBBSSubHelpEntry *entries,
                          size_t count, const char *query)
{
    if (!query || !topic || !entries)
        return;
    if (!lobbsHelpQueryMatches(query, topic) && strcasecmp(query, topic) != 0)
        return;

    if (strcasecmp(query, topic) == 0) {
        for (size_t i = 0; i < count; i++) {
            if (entries[i].line)
                lines.push_back(entries[i].line);
        }
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (!entries[i].verb || !entries[i].line)
            continue;
        char path[80];
        snprintf(path, sizeof(path), "%s %s", topic, entries[i].verb);
        if (strcasecmp(query, path) == 0) {
            lines.push_back(entries[i].line);
            return;
        }
    }
}

bool lobbsHelpStripTrailingPage(char *query, size_t queryCap, uint32_t &pageOut)
{
    (void)queryCap;
    if (!query || !query[0])
        return false;
    char *lastSpace = strrchr(query, ' ');
    if (!lastSpace) {
        uint32_t p = 1;
        if (!lobbsParsePageToken(query, p))
            return false;
        pageOut = p;
        query[0] = '\0';
        return true;
    }
    char *tok = lastSpace + 1;
    uint32_t p = 1;
    if (!lobbsParsePageToken(tok, p))
        return false;
    *lastSpace = '\0';
    while (lastSpace > query && (lastSpace[-1] == ' ' || lastSpace[-1] == '\t'))
        *--lastSpace = '\0';
    pageOut = p;
    return true;
}

static constexpr size_t LOBBS_PAGER_FOOTER_MAX = 16;
static constexpr int LOBBS_PAGER_MAX_PAGES = 64;

static bool lobbsPagerMeasureLine(LoBBSPagerFormatLineFn fn, void *fnCtx, uint32_t index, char *line, size_t lineCap,
                                  size_t &lineLenOut)
{
    if (fn) {
        fn(fnCtx, index, line, lineCap);
        lineLenOut = strlen(line);
        return true;
    }
    lineLenOut = 0;
    return false;
}

static bool lobbsPagerPackPages(uint32_t itemCount, LoBBSPagerFormatLineFn fn, void *fnCtx, size_t maxBytes,
                                bool reserveFooter, uint32_t *pageStarts, int &pageCountOut)
{
    char lineBuf[160];
    pageCountOut = 0;
    if (itemCount == 0)
        return false;
    uint32_t idx = 0;
    while (idx < itemCount && pageCountOut < LOBBS_PAGER_MAX_PAGES) {
        pageStarts[pageCountOut] = idx;
        size_t used = 0;
        while (idx < itemCount) {
            size_t lineLen = 0;
            lobbsPagerMeasureLine(fn, fnCtx, idx, lineBuf, sizeof(lineBuf), lineLen);
            size_t add = lineLen + (used > 0 ? 1 : 0);
            size_t footerReserve = reserveFooter ? LOBBS_PAGER_FOOTER_MAX : 0;
            if (used + add + footerReserve > maxBytes) {
                if (used > 0)
                    break;
                size_t maxLine = maxBytes > footerReserve ? maxBytes - footerReserve : 0;
                if (lineLen > maxLine)
                    lineLen = maxLine;
                lineBuf[lineLen] = '\0';
                used += lineLen;
                idx++;
                break;
            }
            used += add;
            idx++;
            if (idx < itemCount) {
                size_t nextLen = 0;
                lobbsPagerMeasureLine(fn, fnCtx, idx, lineBuf, sizeof(lineBuf), nextLen);
                size_t nextAdd = nextLen + 1;
                if (used + nextAdd + footerReserve > maxBytes)
                    break;
            }
        }
        pageCountOut++;
    }
    if (pageCountOut < LOBBS_PAGER_MAX_PAGES)
        pageStarts[pageCountOut] = itemCount;
    return pageCountOut > 0;
}

static bool lobbsPagerRenderPage(char *out, size_t outCap, uint32_t page1, uint32_t itemCount, LoBBSPagerFormatLineFn fn,
                                 void *fnCtx, const char **errEmpty, const char **errBadPage)
{
    if (itemCount == 0) {
        if (errEmpty)
            *errEmpty = "Empty.";
        return false;
    }
    if (page1 == 0)
        page1 = 1;
    char lineBuf[160];
    uint32_t pageStarts[LOBBS_PAGER_MAX_PAGES + 1];
    int pageCount = 0;
    lobbsPagerPackPages(itemCount, fn, fnCtx, outCap, true, pageStarts, pageCount);
    if (pageCount <= 0) {
        if (errEmpty)
            *errEmpty = "Empty.";
        return false;
    }
    if (pageCount == 1) {
        lobbsPagerPackPages(itemCount, fn, fnCtx, outCap, false, pageStarts, pageCount);
        pageStarts[1] = itemCount;
    }
    if (page1 > (uint32_t)pageCount) {
        if (errBadPage)
            *errBadPage = "No such page.";
        return false;
    }
    uint32_t start = pageStarts[page1 - 1];
    uint32_t end = pageStarts[page1];
    size_t n = 0;
    for (uint32_t i = start; i < end; i++) {
        size_t lineLen = 0;
        lobbsPagerMeasureLine(fn, fnCtx, i, lineBuf, sizeof(lineBuf), lineLen);
        if (i > start && n + 1 < outCap)
            out[n++] = '\n';
        for (size_t c = 0; c < lineLen && n + 1 < outCap; c++)
            out[n++] = lineBuf[c];
    }
    if ((uint32_t)pageCount > 1) {
        char footer[LOBBS_PAGER_FOOTER_MAX];
        int fw = snprintf(footer, sizeof(footer), "{p %u/%u}", page1, (uint32_t)pageCount);
        if (fw > 0 && n + 1 < outCap)
            out[n++] = '\n';
        for (int c = 0; footer[c] && n + 1 < outCap; c++)
            out[n++] = footer[c];
    }
    out[n] = '\0';
    return true;
}

struct LobbsPagerCStringCtx {
    const char *const *lines;
};

static void lobbsPagerFormatCStringLine(void *ctx, uint32_t itemIndex, char *line, size_t lineCap)
{
    auto *p = (LobbsPagerCStringCtx *)ctx;
    if (!p->lines || !p->lines[itemIndex]) {
        line[0] = '\0';
        return;
    }
    strncpy(line, p->lines[itemIndex], lineCap - 1);
    line[lineCap - 1] = '\0';
}

bool lobbsPagerFormatLines(char *out, size_t outCap, uint32_t page1, const char *const *lines, uint32_t lineCount,
                           const char **errEmpty, const char **errBadPage)
{
    LobbsPagerCStringCtx ctx{lines};
    return lobbsPagerRenderPage(out, outCap, page1, lineCount, lobbsPagerFormatCStringLine, &ctx, errEmpty, errBadPage);
}

bool lobbsPagerFormatItems(char *out, size_t outCap, uint32_t page1, uint32_t itemCount, LoBBSPagerFormatLineFn fn,
                           void *fnCtx, const char **errEmpty, const char **errBadPage)
{
    return lobbsPagerRenderPage(out, outCap, page1, itemCount, fn, fnCtx, errEmpty, errBadPage);
}

void lobbsCommandReplyPagedLines(LoBBSCommandCtx &ctx, const std::vector<std::string> &lines)
{
    if (lines.empty()) {
        lobbsCommandReply(ctx, "Empty.");
        return;
    }
    const char *ptrs[256];
    uint32_t n = (uint32_t)lines.size();
    if (n > 256)
        n = 256;
    for (uint32_t i = 0; i < n; i++)
        ptrs[i] = lines[i].c_str();
    char buf[LOBBS_REPLY_BYTES + 1];
    const char *errEmpty = nullptr;
    const char *errBadPage = nullptr;
    if (!lobbsPagerFormatLines(buf, sizeof(buf), ctx.page, ptrs, n, &errEmpty, &errBadPage)) {
        lobbsCommandReply(ctx, errBadPage ? errBadPage : (errEmpty ? errEmpty : "Empty."));
        return;
    }
    lobbsCommandReply(ctx, buf);
}

bool lobbsCtxLoggedIn(const LoBBSCommandCtx &ctx)
{
    return ctx.session.userUuid != 0;
}

uint64_t lobbsCtxUserUuid(const LoBBSCommandCtx &ctx)
{
    return ctx.session.userUuid;
}

bool lobbsCtxUsername(const LoBBSCommandCtx &ctx, char *buf, size_t bufCap)
{
    if (!buf || bufCap == 0)
        return false;
    buf[0] = '\0';
    if (!lobbsCtxLoggedIn(ctx))
        return false;
    strncpy(buf, ctx.session.username, bufCap - 1);
    buf[bufCap - 1] = '\0';
    return buf[0] != '\0';
}

bool lobbsCommandRequireLogin(LoBBSCommandCtx &ctx)
{
    if (lobbsCtxLoggedIn(ctx))
        return true;
    lobbsCommandReply(ctx, "Login required.");
    return false;
}

bool lobbsCommandRequireSysop(LoBBSCommandCtx &ctx)
{
    if (lobbsCtxLoggedIn(ctx) && ctx.session.isSysop)
        return true;
    lobbsCommandReply(ctx, "SysOp only.");
    return false;
}

static bool lobbsPeelSlashLine(char *line, LoBBSCommandCtx &ctx, char **verbOut, char **restOut)
{
    ctx.reqId = 0;
    if (!line || line[0] != '/')
        return false;
    char *p = line + 1;
    if (isdigit((unsigned char)*p)) {
        char *start = p;
        while (isdigit((unsigned char)*p))
            p++;
        if (*p == ' ') {
            *p++ = '\0';
            ctx.reqId = (uint32_t)strtoul(start, nullptr, 10);
            lobbsSkipWs(p);
        } else {
            p = start;
        }
    }
    lobbsSkipWs(p);
    if (!*p)
        return false;
    *verbOut = p;
    while (*p && *p != ' ' && *p != '\t')
        p++;
    if (*p) {
        *p++ = '\0';
        lobbsSkipWs(p);
    }
    *restOut = p;
    return true;
}

void lobbsCommandsHandle(LoBBSModule *mod, const meshtastic_MeshPacket &mp, const LoBBSSession &session, char *line)
{
    LoBBSCommandCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.session = session;
    ctx.page = 1;

    char *verb = nullptr;
    char *rest = nullptr;
    if (!lobbsPeelSlashLine(line, ctx, &verb, &rest))
        return;
    if (!verb[0]) {
        lobbsCommandReply(ctx, "Missing command.");
        return;
    }
    ctx.rest = rest;
    lobbsDoAction("slash_cmd", ctx, verb, rest);
}

#endif
