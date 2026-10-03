#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSCommandRegistry.h"
#include "LoBBSModule.h"
#include "LoBBSReply.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr int LOBBS_MAX_COMMANDS = 16;

struct LobbsCommandEntry {
    const char *name = nullptr;
    LoBBSCommandHandler handler = nullptr;
};

static LobbsCommandEntry lobbsCommandTable[LOBBS_MAX_COMMANDS];
static int lobbsCommandCount = 0;

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

static bool parsePageToken(const char *tok, uint32_t &pageOut)
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

uint32_t lobbsCommandTakePageArg(LoBBSCommandCtx &ctx, uint32_t defaultPage)
{
    if (ctx.argc == 0)
        return defaultPage;
    uint32_t page = defaultPage;
    if (parsePageToken(ctx.argv[ctx.argc - 1], page)) {
        ctx.argc--;
        return page;
    }
    return defaultPage;
}

void lobbsCommandJoinArgs(const LoBBSCommandCtx &ctx, int from, int to, char *out, size_t outCap)
{
    out[0] = '\0';
    size_t n = 0;
    for (int i = from; i < to; i++) {
        if (i > from) {
            if (n + 1 < outCap)
                out[n++] = ' ';
        }
        const char *s = ctx.argv[i];
        while (*s && n + 1 < outCap)
            out[n++] = *s++;
    }
    out[n] = '\0';
}

bool lobbsCommandRequireLogin(LoBBSCommandCtx &ctx)
{
    if (ctx.isAuth && ctx.user)
        return true;
    lobbsCommandReply(ctx, "Login required.");
    return false;
}

bool lobbsCommandRequireSysop(LoBBSCommandCtx &ctx)
{
    if (ctx.isSysop)
        return true;
    lobbsCommandReply(ctx, "SysOp only.");
    return false;
}

bool lobbsCommandNeedArgc(LoBBSCommandCtx &ctx, int min, const char *usage)
{
    if (ctx.argc >= min)
        return true;
    lobbsCommandReply(ctx, usage ? usage : "Usage error.");
    return false;
}

static bool lobbsIsHelpToken(const char *tok)
{
    return tok && tok[0] == '?' && tok[1] == '\0';
}

void lobbsCommandReplySubHelpTopic(LoBBSCommandCtx &ctx, const char *title, const LoBBSSubHelpEntry *entries, size_t count,
                                   const char *verbOrNull)
{
    if (!title)
        title = "Help";
    char buf[LOBBS_REPLY_BYTES + 1];
    if (!verbOrNull) {
        size_t n = 0;
        int w = snprintf(buf, sizeof(buf), "%s\n", title);
        if (w > 0)
            n = (size_t)w;
        bool firstVerb = true;
        for (size_t i = 0; i < count; i++) {
            if (!entries || !entries[i].verb)
                continue;
            if (!firstVerb && n + 2 < sizeof(buf)) {
                buf[n++] = ',';
                buf[n++] = ' ';
            }
            firstVerb = false;
            for (const char *s = entries[i].verb; *s && n + 1 < sizeof(buf); s++)
                buf[n++] = *s;
        }
        buf[n] = '\0';
        lobbsCommandReply(ctx, buf);
        return;
    }
    const char *line = nullptr;
    for (size_t i = 0; i < count; i++) {
        if (entries && entries[i].verb && strcasecmp(entries[i].verb, verbOrNull) == 0) {
            line = entries[i].line;
            break;
        }
    }
    if (line)
        snprintf(buf, sizeof(buf), "%s\n%s", title, line);
    else
        snprintf(buf, sizeof(buf), "%s\nUnknown subcommand.", title);
    lobbsCommandReply(ctx, buf);
}

bool lobbsCommandTrySubHelp(LoBBSCommandCtx &ctx, const char *title, const LoBBSSubHelpEntry *entries, size_t count)
{
    if (!title || !entries || count == 0)
        return false;

    if (ctx.argc >= 2 && lobbsIsHelpToken(ctx.argv[1])) {
        lobbsCommandReplySubHelpTopic(ctx, title, entries, count, nullptr);
        return true;
    }

    if (ctx.argc >= 3 && lobbsIsHelpToken(ctx.argv[2])) {
        lobbsCommandReplySubHelpTopic(ctx, title, entries, count, ctx.argv[1]);
        return true;
    }

    return false;
}

void lobbsCommandDispatchSub(LoBBSCommandCtx &ctx, const LoBBSSubcommand *subs, size_t count, const char *defaultName,
                             const char *unknownReply)
{
    ctx.page = lobbsCommandTakePageArg(ctx);
    const char *verb = defaultName ? defaultName : "list";
    if (ctx.argc >= 2)
        verb = ctx.argv[1];
    for (size_t i = 0; i < count; i++) {
        if (subs[i].name && subs[i].handler && strcasecmp(verb, subs[i].name) == 0) {
            subs[i].handler(ctx);
            return;
        }
    }
    lobbsCommandReply(ctx, unknownReply ? unknownReply : "Unknown command.");
}

void lobbsCommandsInstall(const LoBBSFilterCommands &cmds)
{
    lobbsCommandCount = 0;
    for (int i = 0; i < cmds.count && lobbsCommandCount < LOBBS_MAX_COMMANDS; i++) {
        if (!cmds.slot[i].name || !cmds.slot[i].handler)
            continue;
        lobbsCommandTable[lobbsCommandCount].name = cmds.slot[i].name;
        lobbsCommandTable[lobbsCommandCount].handler = cmds.slot[i].handler;
        lobbsCommandCount++;
    }
}

static bool lobbsTokenize(char *p, LoBBSCommandCtx &ctx)
{
    ctx.argc = 0;
    while (*p && ctx.argc < LOBBS_CMD_MAX_ARGC) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        ctx.argv[ctx.argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
    }
    return ctx.argc > 0;
}

static bool lobbsParseSlash(char *line, LoBBSCommandCtx &ctx)
{
    ctx.reqId = 0;
    ctx.argc = 0;
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
            while (*p == ' ')
                p++;
        } else {
            p = start;
        }
    }
    return lobbsTokenize(p, ctx);
}

void lobbsCommandsHandle(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, bool isAuth,
                         const meshtastic_LoBBSUser *user, bool isSysop, char *line)
{
    LoBBSCommandCtx ctx;
    ctx.mod = mod;
    ctx.mp = &mp;
    ctx.sessionNodeId = sessionNodeId;
    ctx.isAuth = isAuth;
    ctx.user = user;
    ctx.isSysop = isSysop;

    if (!lobbsParseSlash(line, ctx))
        return;
    if (ctx.argc == 0) {
        lobbsCommandReply(ctx, "Missing command.");
        return;
    }

    for (int i = 0; i < lobbsCommandCount; i++) {
        if (strcasecmp(ctx.argv[0], lobbsCommandTable[i].name) == 0) {
            lobbsCommandTable[i].handler(ctx);
            return;
        }
    }
    lobbsCommandReply(ctx, "Unknown command. Try /help");
}

#endif
