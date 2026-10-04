#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSConfig.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "../AppUtil.h"
#include "MailDal.h"
#include "MailRecords.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lodb/LoDB.h>
#include <vector>

static void mailAppendListRecord(LoBBSModule *mod, LoBBSResponse &resp, uint32_t oneBasedIndex, const LoScalar &mail)
{
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char trunc[LOBBS_LIST_LINE_TRUNC_BUFFER_SIZE];
    char msg[LOBBS_MESSAGE_BODY_BUFFER_SIZE];
    char line[LOBBS_REPLY_BYTES + 1];
    char idBuf[24];
    lobbsAppUsernameForUuid(mod, MailDal::mailFromUuid(mail), name, sizeof(name));
    lobbsAppTimeAgo(MailDal::mailTimestamp(mail), when, sizeof(when));
    MailDal::mailMessage(mail, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), LOBBS_LIST_LINE_TRUNC_MAX_CHARS);
    snprintf(line, sizeof(line), "[%u]%s @%s: %s (%s)", (unsigned)oneBasedIndex, MailDal::mailRead(mail) ? "" : "*", name, trunc,
             when);

    LoScalar rec;
    lobbsAppFormatUint64Decimal(idBuf, sizeof(idBuf), MailDal::mailUuid(mail));
    rec.setString(LODB_F_ID, idBuf);
    rec.setString(LODB_F_TITLE, line);
    rec.setString(LODB_F_DESCRIPTION, trunc);
    rec.setBool(MailField::FIELD_READ, MailDal::mailRead(mail));
    rec.setUint32(LODB_F_CREATED, MailDal::mailTimestamp(mail));
    lobbsResponseAppendRecord(resp, rec);
}

static void mailSubList(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);

    uint64_t inboxUuid = lobbsCtxUserUuid(ctx);
    const char *maybeUser = lobbsArgPeek(ctx);
    if (maybeUser && !lobbsTokenIsPage(maybeUser)) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        const char *user = lobbsArgShift(ctx);
        if (!lobbsAppResolveUsername(ctx, user, inboxUuid))
            return;
    }

    auto all = ctx.mod->mail().dal().getAllMailForUser(inboxUuid);
    LoBBSResponse resp;
    if (all.empty()) {
        lobbsResponseSetError(resp, "No mail");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    for (size_t i = 0; i < all.size(); i++)
        mailAppendListRecord(ctx.mod, resp, (uint32_t)(i + 1), all[i]);
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubRead(LoBBSCommandCtx &ctx, bool numericShorthand)
{
    MailDal &mail = ctx.mod->mail().dal();
    if (!numericShorthand) {
        const char *sub = lobbsArgShift(ctx);
        (void)sub;
    }

    uint32_t idx = 0;
    uint64_t inboxUuid = lobbsCtxUserUuid(ctx);
    bool markRead = true;
    bool sysopRead = ctx.session.isSysop && lobbsArgPeek(ctx) && !lobbsArgPeekIsUint(ctx);

    if (sysopRead) {
        const char *user = lobbsArgShift(ctx);
        if (!user || !lobbsAppResolveUsername(ctx, user, inboxUuid))
            return;
        const char *beforeNum = lobbsArgPeek(ctx);
        if (!lobbsArgShiftUint(ctx, idx)) {
            LoBBSResponse resp;
            lobbsResponseSetError(resp, beforeNum ? "Invalid message number." : "Usage: /mail read N");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
        markRead = false;
    } else {
        const char *beforeNum = lobbsArgPeek(ctx);
        if (!lobbsArgShiftUint(ctx, idx)) {
            LoBBSResponse resp;
            lobbsResponseSetError(resp, beforeNum ? "Invalid message number." : "Usage: /mail read N");
            lobbsCommandReplyResponse(ctx, resp);
            return;
        }
    }

    auto mailMessages = mail.getAllMailForUser(inboxUuid);
    if (idx == 0 || idx > mailMessages.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid message number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const LoScalar &m = mailMessages[idx - 1];
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char body[LOBBS_MESSAGE_READ_BODY_BUFFER_SIZE];
    char header[LOBBS_REPLY_BYTES + 1];
    char idBuf[24];
    lobbsAppUsernameForUuid(ctx.mod, MailDal::mailFromUuid(m), name, sizeof(name));
    MailDal::mailMessage(m, body, sizeof(body));
    lobbsAppTimeAgo(MailDal::mailTimestamp(m), when, sizeof(when));
    snprintf(header, sizeof(header), "From: @%s (%s)", name, when);

    LoScalar rec;
    lobbsAppFormatUint64Decimal(idBuf, sizeof(idBuf), MailDal::mailUuid(m));
    rec.setString(LODB_F_ID, idBuf);
    rec.setString(LODB_F_TITLE, header);
    rec.setString(LODB_F_DESCRIPTION, body);
    rec.setUint32(LODB_F_CREATED, MailDal::mailTimestamp(m));
    if (markRead && inboxUuid == lobbsCtxUserUuid(ctx))
        mail.markMailAsRead(MailDal::mailUuid(m));

    LoBBSResponse resp;
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubUnread(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    (void)sub;
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, beforeNum ? "Invalid message number." : "Usage: /mail unread N");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    MailDal &mail = ctx.mod->mail().dal();
    auto mailMessages = mail.getAllMailForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > mailMessages.size()) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Invalid message number");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    bool ok = mail.markMailAsUnread(MailDal::mailUuid(mailMessages[idx - 1]));
    LoBBSResponse resp;
    lobbsRecordPush(resp.records, ok ? "Marked unread." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubDelete(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    (void)sub;
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, beforeNum ? "Invalid message number." : "Usage: /mail delete N");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    LoBBSResponse resp;
    lobbsRecordPush(resp.records,
                    ctx.mod->mail().dal().deleteMailInboxIndex(lobbsCtxUserUuid(ctx), idx) ? "Deleted." : "Failed.");
    lobbsCommandReplyResponse(ctx, resp);
}

static void mailSubSend(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "send") != 0) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /mail send user message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    const char *user = lobbsArgShift(ctx);
    const char *body = lobbsArgRest(ctx);
    if (!user || !body[0]) {
        LoBBSResponse resp;
        lobbsResponseSetError(resp, "Usage: /mail send user message...");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    uint64_t toUuid = 0;
    if (!lobbsAppResolveUsername(ctx, user, toUuid))
        return;
    LoBBSResponse resp;
    lobbsRecordPush(resp.records,
                    ctx.mod->mail().dal().sendMail(lobbsCtxUserUuid(ctx), toUuid, body) ? "Mail sent." : "Failed to send mail.");
    lobbsCommandReplyResponse(ctx, resp);
}

static const LoBBSSubHelpEntry mailHelp[] = {
    {"list", "list — inbox; sysop: list user (then /p2 …)"},
    {"read", "read N — read message (/mail N); sysop: read user N"},
    {"unread", "unread N — mark message unread"},
    {"delete", "delete N — delete message from inbox"},
    {"send", "send user message... — send mail"},
};

static void handleMail(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;

    if (lobbsArgPeekIsUint(ctx)) {
        mailSubRead(ctx, true);
        return;
    }

    const char *sub = lobbsArgPeek(ctx);
    if (!sub || strcasecmp(sub, "list") == 0) {
        mailSubList(ctx);
        return;
    }
    if (strcasecmp(sub, "read") == 0) {
        mailSubRead(ctx, false);
        return;
    }
    if (strcasecmp(sub, "unread") == 0) {
        mailSubUnread(ctx);
        return;
    }
    if (strcasecmp(sub, "delete") == 0) {
        mailSubDelete(ctx);
        return;
    }
    if (strcasecmp(sub, "send") == 0) {
        mailSubSend(ctx);
        return;
    }
    LoBBSResponse resp;
    lobbsResponseSetError(resp, "Unknown command. Try /help mail");
    lobbsCommandReplyResponse(ctx, resp);
}

static void slashMail(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "mail"))
        return;
    handleMail(*ctx);
}

static void filterMailHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)args;
    if (!ctx || !lobbsCtxLoggedIn(*ctx))
        return;
    lobbsRecordPush(topics, "mail", "read and send mail");
}

static void filterMailHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    (void)ctx;
    lobbsHelpForTopic(value, args, "mail", mailHelp, sizeof(mailHelp) / sizeof(mailHelp[0]));
}

static void filterMailStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod)
        return;
    char title[64];
    char value[32];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->mail().dal().countUnreadMail(lobbsCtxUserUuid(*ctx));
        snprintf(title, sizeof(title), "Mail");
        snprintf(value, sizeof(value), "%u unread", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->mail().dal().countAllMail();
        snprintf(title, sizeof(title), "Mail");
        snprintf(value, sizeof(value), "%u total", (unsigned)n);
    }
    lobbsRecordPush(lines, title, value);
}

static bool displayMailRecord(const LoScalar &record, std::string &lineOut)
{
    if (record.has(MailField::FIELD_READ) && record.has(LODB_F_CREATED)) {
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

static void displayMailHuman(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &record)
{
    (void)ctx;
    std::string line;
    if (displayMailRecord(record, line))
        value.setString(LODB_F_TITLE, line);
}

void lobbsMailRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashMail, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterMailHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterMailHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterMailStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("display_human", displayMailHuman, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
