#if !MESHTASTIC_EXCLUDE_LOBBS

#include "NewsCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../AppUtil.h"
#include "../Auth/AuthDal.h"
#include "NewsDal.h"
#include <vector>
#include "news.pb.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct NewsListPagerCtx {
    AuthDal *auth;
    const std::vector<LoBBSNewsEntry> *entries;
};

static void formatNewsListLine(void *ctx, uint32_t itemIndex, char *line, size_t lineCap)
{
    auto *p = (NewsListPagerCtx *)ctx;
    const LoBBSNewsEntry &entry = (*p->entries)[itemIndex];
    const meshtastic_LoBBSNews *news = entry.news;
    meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
    lobbsAppLoadUser(*p->auth, news->author_user_uuid, &author);
    char name[32];
    char when[32];
    char trunc[50];
    lobbsAppCopyCapped(name, sizeof(name), author.username, sizeof(author.username));
    if (!name[0])
        lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
    lobbsAppTimeAgo(news->timestamp, when, sizeof(when));
    lobbsAppTruncMsg(news->message, trunc, sizeof(trunc), 25);
    snprintf(line, lineCap, "[%u]%s @%s: %s (%s)", (unsigned)(itemIndex + 1), entry.isRead ? "" : "*", name, trunc, when);
}

static void freeNewsEntries(std::vector<LoBBSNewsEntry> &all)
{
    for (auto &e : all)
        delete[] (uint8_t *)e.news;
}

static void formatNewsList(LoBBSCommandCtx &ctx, AuthDal &auth, uint64_t readerUuid, uint32_t page1, char *out, size_t outCap,
                           const char **errMsg)
{
    auto all = ctx.mod->news().dal().getAllNewsForUser(readerUuid);
    uint32_t total = (uint32_t)all.size();
    if (total == 0) {
        freeNewsEntries(all);
        *errMsg = "No news";
        return;
    }
    NewsListPagerCtx pagerCtx{&auth, &all};
    const char *errEmpty = nullptr;
    const char *errBadPage = nullptr;
    if (!lobbsPagerFormatItems(out, outCap, page1, total, formatNewsListLine, &pagerCtx, &errEmpty, &errBadPage)) {
        freeNewsEntries(all);
        *errMsg = errBadPage ? errBadPage : (errEmpty ? errEmpty : "No news");
        return;
    }
    freeNewsEntries(all);
    *errMsg = nullptr;
}

static void newsSubList(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    char buf[LOBBS_REPLY_BYTES + 1];
    const char *err = nullptr;
    formatNewsList(ctx, auth, ctx.user->uuid, ctx.page, buf, sizeof(buf), &err);
    lobbsCommandReply(ctx, err ? err : buf);
}

static void newsSubRead(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /news read N"))
        return;
    AuthDal &auth = ctx.mod->auth().dal();
    NewsDal &news = ctx.mod->news().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    auto newsItems = news.getAllNewsForUser(ctx.user->uuid);
    if (idx == 0 || idx > newsItems.size()) {
        for (auto &e : newsItems)
            delete[] (uint8_t *)e.news;
        lobbsCommandReply(ctx, "Invalid news number");
        return;
    }
    const meshtastic_LoBBSNews *item = newsItems[idx - 1].news;
    meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
    lobbsAppLoadUser(auth, item->author_user_uuid, &author);
    char name[32];
    char when[32];
    char body[120];
    lobbsAppCopyCapped(name, sizeof(name), author.username, sizeof(author.username));
    if (!name[0])
        lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
    lobbsAppCopyCapped(body, sizeof(body), item->message, sizeof(item->message));
    lobbsAppTimeAgo(item->timestamp, when, sizeof(when));
    char reply[LOBBS_REPLY_BYTES + 1];
    snprintf(reply, sizeof(reply), "From: @%s (%s)\n%s", name, when, body);
    news.markNewsAsRead(item->uuid, ctx.user->uuid);
    for (auto &e : newsItems)
        delete[] (uint8_t *)e.news;
    lobbsCommandReply(ctx, reply);
}

static void newsSubUnread(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /news unread N"))
        return;
    NewsDal &news = ctx.mod->news().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    auto newsItems = news.getAllNewsForUser(ctx.user->uuid);
    if (idx == 0 || idx > newsItems.size()) {
        for (auto &e : newsItems)
            delete[] (uint8_t *)e.news;
        lobbsCommandReply(ctx, "Invalid news number");
        return;
    }
    uint64_t uuid = newsItems[idx - 1].news->uuid;
    for (auto &e : newsItems)
        delete[] (uint8_t *)e.news;
    lobbsCommandReply(ctx, news.markNewsAsUnread(uuid, ctx.user->uuid) ? "Marked unread." : "Failed.");
}

static void newsSubDelete(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /news delete N"))
        return;
    NewsDal &news = ctx.mod->news().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    auto newsItems = news.getAllNewsForUser(ctx.user->uuid);
    if (idx == 0 || idx > newsItems.size()) {
        for (auto &e : newsItems)
            delete[] (uint8_t *)e.news;
        lobbsCommandReply(ctx, "Invalid news number");
        return;
    }
    const meshtastic_LoBBSNews *item = newsItems[idx - 1].news;
    bool allowed = ctx.isAdmin || item->author_user_uuid == ctx.user->uuid;
    uint64_t uuid = item->uuid;
    for (auto &e : newsItems)
        delete[] (uint8_t *)e.news;
    if (!allowed) {
        lobbsCommandReply(ctx, "Failed.");
        return;
    }
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
    lobbsCommandReply(ctx, ctx.mod->news().dal().postNews(ctx.user->uuid, msgBody) ? "News posted." : "Failed to post news.");
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
    {"delete", "delete N — delete (author or admin)"},
    {"post", "post message... — post news"},
};

static void handleNews(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    if (lobbsCommandTrySubHelp(ctx, "News Help", newsHelp, sizeof(newsHelp) / sizeof(newsHelp[0])))
        return;
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
    uint32_t n = ctx->mod->news().dal().countAllNews();
    snprintf(line, sizeof(line), "News: %u", (unsigned)n);
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
    if (ctx && ctx->isAuth)
        lobbsFilterLinesPush(*(LoBBSFilterLines *)value, "news");
}

void lobbsNewsRegisterCommands()
{
    lobbsRegisterFilter("commands", filterNewsCommands);
    lobbsRegisterFilter("status_lines", filterNewsStatusLines);
    lobbsRegisterFilter("help_topics", filterNewsHelpTopics);
    lobbsRegisterFilter("help_index", filterNewsHelpIndex);
}

#endif
