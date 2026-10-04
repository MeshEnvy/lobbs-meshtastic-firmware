#if !MESHTASTIC_EXCLUDE_LOBBS

#include "NewsCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
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
    char name[32];
    char when[32];
    char trunc[50];
    char msg[201];
    lobbsAppUsernameForUuid(p->mod, NewsDal::newsAuthorUuid(news), name, sizeof(name));
    lobbsAppTimeAgo(NewsDal::newsTimestamp(news), when, sizeof(when));
    NewsDal::newsMessage(news, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), 25);
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
    char buf[LOBBS_REPLY_BYTES + 1];
    const char *err = nullptr;
    formatNewsList(ctx, lobbsCtxUserUuid(ctx), ctx.page, buf, sizeof(buf), &err);
    lobbsCommandReply(ctx, err ? err : buf);
}

static void newsSubRead(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /news read N"))
        return;
    NewsDal &news = ctx.mod->news().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        lobbsCommandReply(ctx, "Invalid news number");
        return;
    }
    const LoScalar &item = newsItems[idx - 1].news;
    char name[32];
    char when[32];
    char body[120];
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
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /news unread N"))
        return;
    NewsDal &news = ctx.mod->news().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
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
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /news delete N"))
        return;
    NewsDal &news = ctx.mod->news().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
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
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /news post message..."))
        return;
    char msgBody[201];
    lobbsCommandJoinArgs(ctx, 2, ctx.argc, msgBody, sizeof(msgBody));
    if (!isalpha((unsigned char)msgBody[0])) {
        lobbsCommandReply(ctx, "Start with a letter.");
        return;
    }
    lobbsCommandReply(ctx, ctx.mod->news().dal().postNews(lobbsCtxUserUuid(ctx), msgBody) ? "News posted."
                                                                                                 : "Failed to post news.");
}

static const LoBBSSubcommand newsSubs[] = {
    {"list", newsSubList},
    {"read", newsSubRead},
    {"unread", newsSubUnread},
    {"delete", newsSubDelete},
    {"post", newsSubPost},
};

static const LoBBSSubHelpEntry newsHelp[] = {
    {"list", "list [pN] — news index"},
    {"read", "read N — read and mark read"},
    {"unread", "unread N — mark unread"},
    {"delete", "delete N — delete (sysop)"},
    {"post", "post message... — post news"},
};

static bool newsRewriteNumericAsRead(LoBBSCommandCtx &ctx)
{
    if (ctx.argc < 2 || !ctx.argv[1])
        return false;
    const char *tok = ctx.argv[1];
    if (!isdigit((unsigned char)tok[0]))
        return false;
    for (const char *s = tok; *s; s++) {
        if (!isdigit((unsigned char)*s))
            return false;
    }
    if (ctx.argc + 1 >= LOBBS_CMD_MAX_ARGC)
        return false;
    for (int i = ctx.argc; i >= 2; i--)
        ctx.argv[i] = ctx.argv[i - 1];
    ctx.argv[1] = (char *)"read";
    ctx.argc++;
    return true;
}

static void handleNews(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    if (lobbsCommandTrySubHelp(ctx, "News Help", newsHelp, sizeof(newsHelp) / sizeof(newsHelp[0])))
        return;
    (void)newsRewriteNumericAsRead(ctx);
    lobbsCommandDispatchSub(ctx, newsSubs, sizeof(newsSubs) / sizeof(newsSubs[0]), "list",
                            "Unknown command. Try /help news");
}

static void filterNewsCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterCommandsAdd(*(LoBBSFilterCommands *)value, "news", handleNews);
}

static void filterNewsStatusLines(void *value, LoBBSCommandCtx *ctx)
{
    if (!ctx || !ctx->mod)
        return;
    char line[LOBBS_FILTER_LINE_BYTES];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->news().dal().countUnreadNews(lobbsCtxUserUuid(*ctx));
        snprintf(line, sizeof(line), "News: %u", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->news().dal().countAllNews();
        snprintf(line, sizeof(line), "News: %u (all time)", (unsigned)n);
    }
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, line);
}

static void filterNewsHelpTopics(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterHelpTopicAdd(*(LoBBSFilterHelpTopics *)value, "news", "News Help", newsHelp,
                            sizeof(newsHelp) / sizeof(newsHelp[0]));
}

static void filterNewsHelpIndex(void *value, LoBBSCommandCtx *ctx)
{
    if (ctx && lobbsCtxLoggedIn(*ctx))
        lobbsFilterLinesPush(*(LoBBSFilterLines *)value, "news");
}

void lobbsNewsRegisterCommands()
{
    LoBBSAppHooks hooks{};
    hooks.commands = filterNewsCommands;
    hooks.status_lines = filterNewsStatusLines;
    hooks.help_topics = filterNewsHelpTopics;
    hooks.help_index = filterNewsHelpIndex;
    hooks.priority = LOBBS_FILTER_PRIORITY_FEATURE;
    lobbsAppRegisterHooks(hooks);
}

#endif
