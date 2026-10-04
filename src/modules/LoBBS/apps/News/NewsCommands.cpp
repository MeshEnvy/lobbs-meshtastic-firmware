#if !MESHTASTIC_EXCLUDE_LOBBS

#include "NewsCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSConfig.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "../AppUtil.h"
#include "NewsDal.h"
#include "NewsRecords.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lodb/LoDB.h>
#include <vector>

static void newsAppendListRecord(LoBBSModule *mod, LoBBSResponse &resp, uint32_t oneBasedIndex, const LoBBSNewsEntry &entry)
{
    const LoScalar &news = entry.news;
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char trunc[LOBBS_LIST_LINE_TRUNC_BUFFER_SIZE];
    char msg[LOBBS_MESSAGE_BODY_BUFFER_SIZE];
    char line[LOBBS_REPLY_BYTES + 1];
    char idBuf[24];
    lobbsAppUsernameForUuid(mod, NewsDal::newsAuthorUuid(news), name, sizeof(name));
    lobbsAppTimeAgo(NewsDal::newsTimestamp(news), when, sizeof(when));
    NewsDal::newsMessage(news, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), LOBBS_LIST_LINE_TRUNC_MAX_CHARS);
    snprintf(line, sizeof(line), "[%u]%s @%s: %s (%s)", (unsigned)oneBasedIndex, entry.isRead ? "" : "*", name, trunc, when);

    LoScalar rec;
    lobbsAppFormatUint64Decimal(idBuf, sizeof(idBuf), NewsDal::newsUuid(news));
    rec.setString(LODB_F_ID, idBuf);
    rec.setString(LODB_F_TITLE, line);
    rec.setString(LODB_F_DESCRIPTION, trunc);
    rec.setBool(NewsField::FIELD_AUTHOR, entry.isRead);
    rec.setUint32(LODB_F_CREATED, NewsDal::newsTimestamp(news));
    lobbsResponseAppendRecord(resp, rec);
}

static void newsSubList(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);

    auto all = ctx.mod->news().dal().getAllNewsForUser(lobbsCtxUserUuid(ctx));
    LoBBSResponse resp;
    if (all.empty()) {
        lobbsResponseSetError(resp, "No news");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    for (size_t i = 0; i < all.size(); i++)
        newsAppendListRecord(ctx.mod, resp, (uint32_t)(i + 1), all[i]);
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubRead(LoBBSCommandCtx &ctx, bool numericShorthand)
{
    if (!numericShorthand)
        lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, beforeNum ? "Invalid news number." : "Usage: /news read N");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    NewsDal &news = ctx.mod->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid news number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const LoScalar &item = newsItems[idx - 1].news;
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char body[LOBBS_MESSAGE_READ_BODY_BUFFER_SIZE];
    char header[LOBBS_REPLY_BYTES + 1];
    char idBuf[24];
    lobbsAppUsernameForUuid(ctx.mod, NewsDal::newsAuthorUuid(item), name, sizeof(name));
    NewsDal::newsMessage(item, body, sizeof(body));
    lobbsAppTimeAgo(NewsDal::newsTimestamp(item), when, sizeof(when));
    snprintf(header, sizeof(header), "From: @%s (%s)", name, when);

    LoScalar rec;
    lobbsAppFormatUint64Decimal(idBuf, sizeof(idBuf), NewsDal::newsUuid(item));
    rec.setString(LODB_F_ID, idBuf);
    rec.setString(LODB_F_TITLE, header);
    rec.setString(LODB_F_DESCRIPTION, body);
    rec.setUint32(LODB_F_CREATED, NewsDal::newsTimestamp(item));
    news.markNewsAsRead(NewsDal::newsUuid(item), lobbsCtxUserUuid(ctx));

    LoBBSResponse resp;
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubUnread(LoBBSCommandCtx &ctx)
{
    lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, beforeNum ? "Invalid news number." : "Usage: /news unread N");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    NewsDal &news = ctx.mod->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid news number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    uint64_t uuid = NewsDal::newsUuid(newsItems[idx - 1].news);
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, news.markNewsAsUnread(uuid, lobbsCtxUserUuid(ctx)) ? "Marked unread." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubDelete(LoBBSCommandCtx &ctx)
{
    lobbsArgShift(ctx);
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, beforeNum ? "Invalid news number." : "Usage: /news delete N");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    NewsDal &news = ctx.mod->news().dal();
    auto newsItems = news.getAllNewsForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > newsItems.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid news number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    if (!ctx.session.isSysop) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "SysOp only.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    uint64_t uuid = NewsDal::newsUuid(newsItems[idx - 1].news);
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, news.deleteNewsUuid(uuid) ? "Deleted." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void newsSubPost(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "post") != 0) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /news post message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const char *msgBody = lobbsArgRest(ctx);
    if (!msgBody[0]) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /news post message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    if (!isalpha((unsigned char)msgBody[0])) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Start with a letter.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    LoBBSResponse resp;
    lobbsRecordPush(resp.records,
                    ctx.mod->news().dal().postNews(lobbsCtxUserUuid(ctx), msgBody) ? "News posted." : "Failed to post news.");
    lobbsCommandReplyResponse(ctx, resp);
}

static const LoBBSSubHelpEntry newsHelp[] = {
    {"list", "list — news index (/p2 …)"},   {"read", "read N — read and mark read"}, {"unread", "unread N — mark unread"},
    {"delete", "delete N — delete (sysop)"}, {"post", "post message... — post news"},
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
    LoBBSResponse resp;
    lobbsResponseSetError(resp, "Unknown command. Try /help news");
    lobbsCommandReplyResponse(ctx, resp);
}

static void slashNews(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "news"))
        return;
    handleNews(*ctx);
}

static void filterNewsHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)args;
    if (!ctx || !lobbsCtxLoggedIn(*ctx))
        return;
    lobbsRecordPush(topics, "news", "read and post news");
}

static void filterNewsHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    (void)ctx;
    lobbsHelpForTopic(value, args, "news", newsHelp, sizeof(newsHelp) / sizeof(newsHelp[0]));
}

static void filterNewsStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod)
        return;
    char title[64];
    char value[32];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->news().dal().countUnreadNews(lobbsCtxUserUuid(*ctx));
        snprintf(title, sizeof(title), "News");
        snprintf(value, sizeof(value), "%u unread", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->news().dal().countAllNews();
        snprintf(title, sizeof(title), "News");
        snprintf(value, sizeof(value), "%u total", (unsigned)n);
    }
    lobbsRecordPush(lines, title, value);
}

static bool displayNewsRecord(const LoScalar &record, std::string &lineOut)
{
    if (record.has(NewsField::FIELD_AUTHOR) && record.has(LODB_F_CREATED)) {
        std::string title;
        if (record.getString(LODB_F_TITLE, title)) {
            lineOut = title;
            return true;
        }
    }
    if (record.has(LODB_F_DESCRIPTION) && record.has(LODB_F_TITLE)) {
        std::string title;
        std::string body;
        if (record.getString(LODB_F_TITLE, title) && record.getString(LODB_F_DESCRIPTION, body) && title.find("From:") == 0) {
            lineOut = title + "\n" + body;
            return true;
        }
    }
    return false;
}

static void displayNewsHuman(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &record)
{
    (void)ctx;
    std::string line;
    if (displayNewsRecord(record, line))
        value.setString(LODB_F_TITLE, line);
}

void lobbsNewsRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashNews, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterNewsHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterNewsHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterNewsStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("display_human", displayNewsHuman, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
