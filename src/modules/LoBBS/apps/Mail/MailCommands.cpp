#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSConfig.h"
#include "../../LoBBSReply.h"
#include "../AppUtil.h"
#include "MailDal.h"
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
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char trunc[LOBBS_LIST_LINE_TRUNC_BUFFER_SIZE];
    char msg[LOBBS_MESSAGE_BODY_BUFFER_SIZE];
    lobbsAppUsernameForUuid(p->mod, MailDal::mailFromUuid(mail), name, sizeof(name));
    lobbsAppTimeAgo(MailDal::mailTimestamp(mail), when, sizeof(when));
    MailDal::mailMessage(mail, msg, sizeof(msg));
    lobbsAppTruncMsg(msg, trunc, sizeof(trunc), LOBBS_LIST_LINE_TRUNC_MAX_CHARS);
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
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);
    lobbsArgTakePage(ctx);

    uint64_t inboxUuid = lobbsCtxUserUuid(ctx);
    const char *maybeUser = lobbsArgPeek(ctx);
    if (maybeUser && !lobbsTokenIsPage(maybeUser)) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        const char *user = lobbsArgShift(ctx);
        if (!lobbsAppResolveUsername(ctx, user, inboxUuid))
            return;
        lobbsArgTakePage(ctx);
    }

    char buf[LOBBS_REPLY_BYTES + 1];
    const char *err = nullptr;
    formatMailList(ctx, inboxUuid, ctx.page, buf, sizeof(buf), &err);
    lobbsCommandReply(ctx, err ? err : buf);
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
            lobbsCommandReply(ctx, beforeNum ? "Invalid message number." : "Usage: /mail read N");
            return;
        }
        markRead = false;
    } else {
        const char *beforeNum = lobbsArgPeek(ctx);
        if (!lobbsArgShiftUint(ctx, idx)) {
            lobbsCommandReply(ctx, beforeNum ? "Invalid message number." : "Usage: /mail read N");
            return;
        }
    }

    auto mailMessages = mail.getAllMailForUser(inboxUuid);
    if (idx == 0 || idx > mailMessages.size()) {
        lobbsCommandReply(ctx, "Invalid message number");
        return;
    }
    const LoScalar &m = mailMessages[idx - 1];
    char name[LOBBS_USERNAME_BUFFER_SIZE];
    char when[LOBBS_TIME_AGO_BUFFER_SIZE];
    char body[LOBBS_MESSAGE_READ_BODY_BUFFER_SIZE];
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
    const char *sub = lobbsArgShift(ctx);
    (void)sub;
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        lobbsCommandReply(ctx, beforeNum ? "Invalid message number." : "Usage: /mail unread N");
        return;
    }
    MailDal &mail = ctx.mod->mail().dal();
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
    const char *sub = lobbsArgShift(ctx);
    (void)sub;
    const char *beforeNum = lobbsArgPeek(ctx);
    uint32_t idx = 0;
    if (!lobbsArgShiftUint(ctx, idx)) {
        lobbsCommandReply(ctx, beforeNum ? "Invalid message number." : "Usage: /mail delete N");
        return;
    }
    lobbsCommandReply(ctx,
                      ctx.mod->mail().dal().deleteMailInboxIndex(lobbsCtxUserUuid(ctx), idx) ? "Deleted." : "Failed.");
}

static void mailSubSend(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "send") != 0) {
        lobbsCommandReply(ctx, "Usage: /mail send user message...");
        return;
    }
    const char *user = lobbsArgShift(ctx);
    const char *body = lobbsArgRest(ctx);
    if (!user || !body[0]) {
        lobbsCommandReply(ctx, "Usage: /mail send user message...");
        return;
    }
    uint64_t toUuid = 0;
    if (!lobbsAppResolveUsername(ctx, user, toUuid))
        return;
    lobbsCommandReply(ctx, ctx.mod->mail().dal().sendMail(lobbsCtxUserUuid(ctx), toUuid, body) ? "Mail sent."
                                                                                                   : "Failed to send mail.");
}

static const LoBBSSubHelpEntry mailHelp[] = {
    {"list", "list [pN] — inbox; sysop: list user [pN]"},
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
    lobbsCommandReply(ctx, "Unknown command. Try /help mail");
}

static void slashMail(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb || strcasecmp(verb, "mail") != 0)
        return;
    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;
    handleMail(c);
}

static void filterMailRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    if (query || !ctx || !lobbsCtxLoggedIn(*ctx))
        return;
    lobbsRootCommandPush(lines, "mail");
}

static void filterMailCommandHelp(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    lobbsCommandHelpPush(lines, "mail", mailHelp, sizeof(mailHelp) / sizeof(mailHelp[0]), query);
}

static void filterMailStatusLines(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)query;
    if (!ctx || !ctx->mod)
        return;
    char line[64];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->mail().dal().countUnreadMail(lobbsCtxUserUuid(*ctx));
        snprintf(line, sizeof(line), "Mail: %u", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->mail().dal().countAllMail();
        snprintf(line, sizeof(line), "Mail: %u (all time)", (unsigned)n);
    }
    lines.push_back(line);
}

void lobbsMailRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashMail, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("root_commands", filterMailRootCommands, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("command_help", filterMailCommandHelp, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterMailStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
