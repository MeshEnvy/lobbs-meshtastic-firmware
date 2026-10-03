#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../AppUtil.h"
#include "MailDal.h"
#include <cctype>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <cstring>

struct MailListPagerCtx {
    LoBBSModule *mod;
    const std::vector<LoScalar> *records;
};

static void formatMailListLine(void *ctx, uint32_t itemIndex, char *line, size_t lineCap)
{
    auto *p = (MailListPagerCtx *)ctx;
    const LoScalar &mail = (*p->records)[itemIndex];
    char name[32];
    char when[32];
    char trunc[50];
    char msg[201];
    lobbsAppUsernameForUuid(p->mod, MailDal::mailFromUuid(mail), name, sizeof(name));
    lobbsAppTimeAgo(MailDal::mailTimestamp(mail), when, sizeof(when));
    MailDal::mailMessage(mail, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), 25);
    snprintf(line, lineCap, "[%u]%s @%s: %s (%s)", (unsigned)(itemIndex + 1), MailDal::mailRead(mail) ? "" : "*", name,
             trunc, when);
}

static void formatMailList(LoBBSCommandCtx &ctx, uint64_t inboxUuid, uint32_t page1, char *out, size_t outCap,
                           const char **errMsg)
{
    auto all = ctx.mod->mail().dal().getAllMailForUser(inboxUuid);
    uint32_t total = (uint32_t)all.size();
    if (total == 0) {
        *errMsg = "No mail";
        return;
    }
    MailListPagerCtx pagerCtx{ctx.mod, &all};
    const char *errEmpty = nullptr;
    const char *errBadPage = nullptr;
    if (!lobbsPagerFormatItems(out, outCap, page1, total, formatMailListLine, &pagerCtx, &errEmpty, &errBadPage)) {
        *errMsg = errBadPage ? errBadPage : (errEmpty ? errEmpty : "No mail");
        return;
    }
    *errMsg = nullptr;
}

static void mailSubList(LoBBSCommandCtx &ctx)
{
    uint64_t inboxUuid = lobbsCtxUserUuid(ctx);
    if (ctx.argc >= 3) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        if (!lobbsAppResolveUsername(ctx, ctx.argv[2], inboxUuid))
            return;
    }
    char buf[LOBBS_REPLY_BYTES + 1];
    const char *err = nullptr;
    formatMailList(ctx, inboxUuid, ctx.page, buf, sizeof(buf), &err);
    lobbsCommandReply(ctx, err ? err : buf);
}

static void mailSubRead(LoBBSCommandCtx &ctx)
{
    MailDal &mail = ctx.mod->mail().dal();
    bool sysopRead = ctx.session.isSysop && ctx.argc >= 4;
    uint32_t idx = 0;
    uint64_t inboxUuid = lobbsCtxUserUuid(ctx);
    bool markRead = true;
    if (sysopRead) {
        if (!lobbsAppResolveUsername(ctx, ctx.argv[2], inboxUuid))
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
        lobbsCommandReply(ctx, "Invalid message number");
        return;
    }
    const LoScalar &m = mailMessages[idx - 1];
    char name[32];
    char when[32];
    char body[120];
    lobbsAppUsernameForUuid(ctx.mod, MailDal::mailFromUuid(m), name, sizeof(name));
    MailDal::mailMessage(m, body, sizeof(body));
    lobbsAppTimeAgo(MailDal::mailTimestamp(m), when, sizeof(when));
    char reply[LOBBS_REPLY_BYTES + 1];
    snprintf(reply, sizeof(reply), "From: @%s (%s)\n%s", name, when, body);
    if (markRead && inboxUuid == lobbsCtxUserUuid(ctx))
        mail.markMailAsRead(MailDal::mailUuid(m));
    lobbsCommandReply(ctx, reply);
}

static void mailSubUnread(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /mail unread N"))
        return;
    MailDal &mail = ctx.mod->mail().dal();
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    auto mailMessages = mail.getAllMailForUser(lobbsCtxUserUuid(ctx));
    if (idx == 0 || idx > mailMessages.size()) {
        lobbsCommandReply(ctx, "Invalid message number");
        return;
    }
    bool ok = mail.markMailAsUnread(MailDal::mailUuid(mailMessages[idx - 1]));
    lobbsCommandReply(ctx, ok ? "Marked unread." : "Failed.");
}

static void mailSubDelete(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /mail delete N"))
        return;
    uint32_t idx = (uint32_t)atoi(ctx.argv[2]);
    lobbsCommandReply(ctx,
                      ctx.mod->mail().dal().deleteMailInboxIndex(lobbsCtxUserUuid(ctx), idx) ? "Deleted." : "Failed.");
}

static void mailSubSend(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandNeedArgc(ctx, 4, "Usage: /mail send user message..."))
        return;
    uint64_t toUuid = 0;
    if (!lobbsAppResolveUsername(ctx, ctx.argv[2], toUuid))
        return;
    char msgBody[201];
    lobbsCommandJoinArgs(ctx, 3, ctx.argc, msgBody, sizeof(msgBody));
    lobbsCommandReply(ctx, ctx.mod->mail().dal().sendMail(lobbsCtxUserUuid(ctx), toUuid, msgBody) ? "Mail sent."
                                                                                                         : "Failed to send mail.");
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
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->mail().dal().countUnreadMail(lobbsCtxUserUuid(*ctx));
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
    if (ctx && lobbsCtxLoggedIn(*ctx))
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
