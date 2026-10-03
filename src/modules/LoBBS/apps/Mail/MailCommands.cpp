#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../AppUtil.h"
#include "../Auth/AuthDal.h"
#include "MailDal.h"
#include "mail.pb.h"
#include <lodb/LoDB.h>
#include <cctype>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <cstring>

static bool resolveUser(LoBBSCommandCtx &ctx, AuthDal &auth, const char *username, uint64_t &uuidOut)
{
    uuidOut = auth.getUserUuidByUsername(username);
    if (uuidOut != 0)
        return true;
    char buf[64];
    snprintf(buf, sizeof(buf), "User '%s' not found.", username);
    lobbsCommandReply(ctx, buf);
    return false;
}

struct MailListPagerCtx {
    AuthDal *auth;
    const std::vector<void *> *records;
};

static void formatMailListLine(void *ctx, uint32_t itemIndex, char *line, size_t lineCap)
{
    auto *p = (MailListPagerCtx *)ctx;
    const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)(*p->records)[itemIndex];
    meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
    lobbsAppLoadUser(*p->auth, mail->from_user_uuid, &sender);
    char name[32];
    char when[32];
    char trunc[50];
    lobbsAppCopyCapped(name, sizeof(name), sender.username, sizeof(sender.username));
    if (!name[0])
        lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
    lobbsAppTimeAgo(mail->timestamp, when, sizeof(when));
    lobbsAppTruncMsg(mail->message, trunc, sizeof(trunc), 25);
    snprintf(line, lineCap, "[%u]%s @%s: %s (%s)", (unsigned)(itemIndex + 1), mail->read ? "" : "*", name, trunc, when);
}

static void formatMailList(LoBBSCommandCtx &ctx, AuthDal &auth, uint64_t inboxUuid, uint32_t page1, char *out, size_t outCap,
                           const char **errMsg)
{
    auto all = ctx.mod->mail().dal().getAllMailForUser(inboxUuid);
    uint32_t total = (uint32_t)all.size();
    if (total == 0) {
        LoDb::freeRecords(all);
        *errMsg = "No mail";
        return;
    }
    MailListPagerCtx pagerCtx{&auth, &all};
    const char *errEmpty = nullptr;
    const char *errBadPage = nullptr;
    if (!lobbsPagerFormatItems(out, outCap, page1, total, formatMailListLine, &pagerCtx, &errEmpty, &errBadPage)) {
        LoDb::freeRecords(all);
        *errMsg = errBadPage ? errBadPage : (errEmpty ? errEmpty : "No mail");
        return;
    }
    LoDb::freeRecords(all);
    *errMsg = nullptr;
}

static void mailSubList(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    uint64_t inboxUuid = ctx.user->uuid;
    if (ctx.argc >= 3) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        if (!resolveUser(ctx, auth, ctx.argv[2], inboxUuid))
            return;
    }
    char buf[LOBBS_REPLY_BYTES + 1];
    const char *err = nullptr;
    formatMailList(ctx, auth, inboxUuid, ctx.page, buf, sizeof(buf), &err);
    lobbsCommandReply(ctx, err ? err : buf);
}

static void mailSubRead(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    MailDal &mail = ctx.mod->mail().dal();
    bool sysopRead = ctx.isSysop && ctx.argc >= 4;
    uint32_t idx = 0;
    uint64_t inboxUuid = ctx.user->uuid;
    bool markRead = true;
    if (sysopRead) {
        if (!resolveUser(ctx, auth, ctx.argv[2], inboxUuid))
            return;
        idx = (uint32_t)atoi(ctx.argv[3]);
        markRead = false;
    } else if (ctx.argc >= 3) {
        idx = (uint32_t)atoi(ctx.argv[2]);
    } else {
        lobbsCommandReply(ctx, "Usage: /mail read N");
        return;
    }
    auto mailMessages = mail.getAllMailForUser(inboxUuid);
    if (idx == 0 || idx > mailMessages.size()) {
        LoDb::freeRecords(mailMessages);
        lobbsCommandReply(ctx, "Invalid message number");
        return;
    }
    const meshtastic_LoBBSMail *m = (const meshtastic_LoBBSMail *)mailMessages[idx - 1];
    meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
    lobbsAppLoadUser(auth, m->from_user_uuid, &sender);
    char name[32];
    char when[32];
    char body[120];
    lobbsAppCopyCapped(name, sizeof(name), sender.username, sizeof(sender.username));
    if (!name[0])
        lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
    lobbsAppCopyCapped(body, sizeof(body), m->message, sizeof(m->message));
    lobbsAppTimeAgo(m->timestamp, when, sizeof(when));
    char reply[LOBBS_REPLY_BYTES + 1];
    snprintf(reply, sizeof(reply), "From: @%s (%s)\n%s", name, when, body);
    if (markRead && inboxUuid == ctx.user->uuid)
        mail.markMailAsRead(m->uuid);
    LoDb::freeRecords(mailMessages);
    lobbsCommandReply(ctx, reply);
}

static void mailSubUnread(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /mail unread N"))
        return;
    MailDal &mail = ctx.mod->mail().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    auto mailMessages = mail.getAllMailForUser(ctx.user->uuid);
    if (idx == 0 || idx > mailMessages.size()) {
        LoDb::freeRecords(mailMessages);
        lobbsCommandReply(ctx, "Invalid message number");
        return;
    }
    const meshtastic_LoBBSMail *m = (const meshtastic_LoBBSMail *)mailMessages[idx - 1];
    bool ok = mail.markMailAsUnread(m->uuid);
    LoDb::freeRecords(mailMessages);
    lobbsCommandReply(ctx, ok ? "Marked unread." : "Failed.");
}

static void mailSubDelete(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /mail delete N"))
        return;
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    lobbsCommandReply(ctx, ctx.mod->mail().dal().deleteMailInboxIndex(ctx.user->uuid, idx) ? "Deleted." : "Failed.");
}

static void mailSubSend(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 4, "Usage: /mail send user message..."))
        return;
    AuthDal &auth = ctx.mod->auth().dal();
    uint64_t toUuid = 0;
    if (!resolveUser(ctx, auth, ctx.argv[2], toUuid))
        return;
    char msgBody[201];
    lobbsCommandJoinArgs(ctx, 3, ctx.argc, msgBody, sizeof(msgBody));
    lobbsCommandReply(ctx, ctx.mod->mail().dal().sendMail(ctx.user->uuid, toUuid, msgBody) ? "Mail sent." : "Failed to send mail.");
}

static const LoBBSSubcommand mailSubs[] = {
    {"list", mailSubList},
    {"read", mailSubRead},
    {"unread", mailSubUnread},
    {"delete", mailSubDelete},
    {"send", mailSubSend},
};

static const LoBBSSubHelpEntry mailHelp[] = {
    {"list", "list [pN] — inbox; sysop: list user [pN]"},
    {"read", "read N — read message (/mail N); sysop: read user N"},
    {"unread", "unread N — mark message unread"},
    {"delete", "delete N — delete message from inbox"},
    {"send", "send user message... — send mail"},
};

static bool mailRewriteNumericAsRead(LoBBSCommandCtx &ctx)
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

static void handleMail(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    if (lobbsCommandTrySubHelp(ctx, "Mail Help", mailHelp, sizeof(mailHelp) / sizeof(mailHelp[0])))
        return;
    (void)mailRewriteNumericAsRead(ctx);
    lobbsCommandDispatchSub(ctx, mailSubs, sizeof(mailSubs) / sizeof(mailSubs[0]), "list",
                            "Unknown command. Try /help mail");
}

static void filterMailCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterCommandsAdd(*(LoBBSFilterCommands *)value, "mail", handleMail);
}

static void filterMailStatusLines(void *value, LoBBSCommandCtx *ctx)
{
    if (!ctx || !ctx->mod)
        return;
    char line[LOBBS_FILTER_LINE_BYTES];
    if (ctx->isAuth) {
        if (!ctx->user)
            return;
        uint32_t n = ctx->mod->mail().dal().countUnreadMail(ctx->user->uuid);
        snprintf(line, sizeof(line), "Mail: %u", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->mail().dal().countAllMail();
        snprintf(line, sizeof(line), "Mail: %u (all time)", (unsigned)n);
    }
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, line);
}

static void filterMailHelpTopics(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterHelpTopicAdd(*(LoBBSFilterHelpTopics *)value, "mail", "Mail Help", mailHelp,
                            sizeof(mailHelp) / sizeof(mailHelp[0]));
}

static void filterMailHelpIndex(void *value, LoBBSCommandCtx *ctx)
{
    if (ctx && ctx->isAuth)
        lobbsFilterLinesPush(*(LoBBSFilterLines *)value, "mail");
}

void lobbsMailRegisterCommands()
{
    lobbsRegisterFilter("commands", filterMailCommands);
    lobbsRegisterFilter("status_lines", filterMailStatusLines);
    lobbsRegisterFilter("help_topics", filterMailHelpTopics);
    lobbsRegisterFilter("help_index", filterMailHelpIndex);
}

#endif
