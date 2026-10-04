#if !MESHTASTIC_EXCLUDE_LOBBS

#include "NewsCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSConfig.h"
#include "../../LoBBSReply.h"
#include "../AppUtil.h"
#include "NewsDal.h"
#include <vector>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct NewsListPagerCtx {
    LoBBSModule *mod;
    const std::vector<LoBBSNewsEntry> *entries;
};

static void formatNewsListLine(void *ctx, uint32_t itemIndex, char *line, size_t lineCap)
{
    auto *p = (NewsListPagerCtx *)ctx;
    const LoBBSNewsEntry &entry = (*p->entries)[itemIndex];
    const LoScalar &news = entry.news;
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char trunc[LOBBS_LIST_LINE_TRUNC_BUFFER_SIZE];
    char msg[LOBBS_MESSAGE_BODY_BUFFER_SIZE];
    lobbsAppUsernameForUuid(p->mod, NewsDal::newsAuthorUuid(news), name, sizeof(name));
    lobbsAppTimeAgo(NewsDal::newsTimestamp(news), when, sizeof(when));
    NewsDal::newsMessage(news, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), LOBBS_LIST_LINE_TRUNC_MAX_CHARS);
    snprintf(line, lineCap, "[%u]%s @%s: %s (%s)", (unsigned)(itemIndex + 1), entry.isRead ? "" : "*", name, trunc, when);
}

static void formatNewsList(LoBBSCommandCtx &ctx, uint64_t readerUuid, uint32_t page1, char *out, size_t outCap,
                           const char **errMsg)
{
    auto all = ctx.mod->news().dal().getAllNewsForUser(readerUuid);
    uint32_t total = (uint32_t)all.size();
    if (total == 0) {
        *errMsg = "No news";
        return;
    }
    NewsListPagerCtx pagerCtx{ctx.mod, &all};
    const char *errEmpty = nullptr;
    const char *errBadPage = nullptr;
    if (!lobbsPagerFormatItems(out, outCap, page1, total, formatNewsListLine, &pagerCtx, &errEmpty, &errBadPage)) {
        *errMsg = errBadPage ? errBadPage : (errEmpty ? errEmpty : "No news");
        return;
    }
    *errMsg = nullptr;
}

static void newsSubList(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);
    lobbsArgTakePage(ctx);
    char buf[LOBBS_REPLY_BYTES + 1];
    const char *err = nullptr;
    formatNewsList(ctx, lobbsCtxUserUuid(ctx), ctx.page, buf, sizeof(buf), &err);
    lobbsCommandReply(ctx, err ? err : buf);
}

static void newsSubRead(LoBBSCommandCtx &ctx, bool numericShorthand)
{
    if (!numericShorthand)
        lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        lobbsCommandReply(ctx, beforeNum ? "Invalid news number." : "Usage: /news read N");
        return;
    }
    NewsDal &news = ctx.mod->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        lobbsCommandReply(ctx, "Invalid news number");
        return;
    }
    const LoScalar &item = newsItems[idx - 1].news;
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char body[LOBBS_MESSAGE_READ_BODY_BUFFER_SIZE];
    lobbsAppUsernameForUuid(ctx.mod, NewsDal::newsAuthorUuid(item), name, sizeof(name));
    NewsDal::newsMessage(item, body, sizeof(body));
    lobbsAppTimeAgo(NewsDal::newsTimestamp(item), when, sizeof(when));
    char reply[LOBBS_REPLY_BYTES + 1];
    snprintf(reply, sizeof(reply), "From: @%s (%s)\n%s", name, when, body);
    news.markNewsAsRead(NewsDal::newsUuid(item), lobbsCtxUserUuid(ctx));
    lobbsCommandReply(ctx, reply);
}

static void newsSubUnread(LoBBSCommandCtx &ctx)
{
    lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        lobbsCommandReply(ctx, beforeNum ? "Invalid news number." : "Usage: /news unread N");
        return;
    }
    NewsDal &news = ctx.mod->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        lobbsCommandReply(ctx, "Invalid news number");
        return;
    }
    uint64_t uuid = NewsDal::newsUuid(newsItems[idx - 1].news);
    lobbsCommandReply(ctx, news.markNewsAsUnread(uuid, lobbsCtxUserUuid(ctx)) ? "Marked unread." : "Failed.");
}

static void newsSubDelete(LoBBSCommandCtx &ctx)
{
    lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        lobbsCommandReply(ctx, beforeNum ? "Invalid news number." : "Usage: /news delete N");
        return;
    }
    NewsDal &news = ctx.mod->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        lobbsCommandReply(ctx, "Invalid news number");
        return;
    }
    if (!ctx.session.isSysop) {
        lobbsCommandReply(ctx, "SysOp only.");
        return;
    }
    uint64_t uuid = NewsDal::newsUuid(newsItems[idx - 1].news);
    lobbsCommandReply(ctx, news.deleteNewsUuid(uuid) ? "Deleted." : "Failed.");
}

static void newsSubPost(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "post") != 0) {
        lobbsCommandReply(ctx, "Usage: /news post message...");
        return;
    }
    const char *msgBody = lobbsArgRest(ctx);
    if (!msgBody[0]) {
        lobbsCommandReply(ctx, "Usage: /news post message...");
        return;
    }
    if (!isalpha((unsigned char)msgBody[0])) {
        lobbsCommandReply(ctx, "Start with a letter.");
        return;
    }
    lobbsCommandReply(ctx, ctx.mod->news().dal().postNews(lobbsCtxUserUuid(ctx), msgBody) ? "News posted."
                                                                                             : "Failed to post news.");
}

static const LoBBSSubHelpEntry newsHelp[] = {
    {"list", "list [pN] — news index"},
    {"read", "read N — read and mark read"},
    {"unread", "unread N — mark unread"},
    {"delete", "delete N — delete (sysop)"},
    {"post", "post message... — post news"},
};

static void handleNews(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;

    if (lobbsArgPeekIsUint(ctx)) {
        newsSubRead(ctx, true);
        return;
    }

    const char *sub = lobbsArgPeek(ctx);
    if (!sub || strcasecmp(sub, "list") == 0) {
        newsSubList(ctx);
        return;
    }
    if (strcasecmp(sub, "read") == 0) {
        newsSubRead(ctx, false);
        return;
    }
    if (strcasecmp(sub, "unread") == 0) {
        newsSubUnread(ctx);
        return;
    }
    if (strcasecmp(sub, "delete") == 0) {
        newsSubDelete(ctx);
        return;
    }
    if (strcasecmp(sub, "post") == 0) {
        newsSubPost(ctx);
        return;
    }
    lobbsCommandReply(ctx, "Unknown command. Try /help news");
}

static void slashNews(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb || strcasecmp(verb, "news") != 0)
        return;
    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;
    handleNews(c);
}

static void filterNewsRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    if (query || !ctx || !lobbsCtxLoggedIn(*ctx))
        return;
    lobbsRootCommandPush(lines, "news");
}

static void filterNewsCommandHelp(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    lobbsCommandHelpPush(lines, "news", newsHelp, sizeof(newsHelp) / sizeof(newsHelp[0]), query);
}

static void filterNewsStatusLines(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)query;
    if (!ctx || !ctx->mod)
        return;
    char line[64];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->news().dal().countUnreadNews(lobbsCtxUserUuid(*ctx));
        snprintf(line, sizeof(line), "News: %u", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->news().dal().countAllNews();
        snprintf(line, sizeof(line), "News: %u (all time)", (unsigned)n);
    }
    lines.push_back(line);
}

void lobbsNewsRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashNews, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("root_commands", filterNewsRootCommands, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("command_help", filterNewsCommandHelp, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterNewsStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
