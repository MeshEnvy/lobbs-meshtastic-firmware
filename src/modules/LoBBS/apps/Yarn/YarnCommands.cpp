#if !MESHTASTIC_EXCLUDE_LOBBS

#include "YarnCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "YarnDal.h"
#include <cstdio>
#include <cstring>
#include <strings.h>

static const LoBBSSubHelpEntry yarnHelp[] = {
    {"view", "view — /yarn shows the tail (no login)"},
    {"add", "add — /yarn word … (login, quota)"},
    {"limit", "limit — sysop: /yarn limit SEC WORDS CHARS"},
};

static void replyYarnView(LoBBSCommandCtx &ctx, bool markSeen)
{
    char buf[LOBBS_REPLY_BYTES + 1];
    if (!ctx.mod->yarn().dal().formatYarnView(buf, sizeof(buf))) {
        lobbsCommandReply(ctx, "Yarn error.");
        return;
    }
    if (markSeen && lobbsCtxLoggedIn(ctx))
        ctx.mod->yarn().dal().markYarnSeen(lobbsCtxUserUuid(ctx));
    lobbsCommandReply(ctx, buf);
}

static void yarnSubLimit(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    lobbsArgShift(ctx);
    uint32_t period = 0;
    uint32_t words = 0;
    uint32_t chars = 0;
    if (!lobbsArgShiftUint(ctx, period) || !lobbsArgShiftUint(ctx, words) || !lobbsArgShiftUint(ctx, chars) ||
        lobbsArgHasMore(ctx)) {
        lobbsCommandReply(ctx, "Usage: /yarn limit SEC WORDS CHARS");
        return;
    }
    char err[48];
    if (!ctx.mod->yarn().dal().setConfig(period, words, chars, err, sizeof(err))) {
        lobbsCommandReply(ctx, err[0] ? err : "Failed.");
        return;
    }
    char reply[72];
    snprintf(reply, sizeof(reply), "Limit %us %u words %u chars.", (unsigned)period, (unsigned)words, (unsigned)chars);
    lobbsCommandReply(ctx, reply);
}

static void handleYarn(LoBBSCommandCtx &ctx)
{
    const char *peek = lobbsArgPeek(ctx);
    if (!peek) {
        replyYarnView(ctx, true);
        return;
    }

    if (strcasecmp(peek, "limit") == 0) {
        yarnSubLimit(ctx);
        return;
    }

    if (!lobbsCommandRequireLogin(ctx))
        return;

    const char *toks[32];
    int n = lobbsArgShiftMany(ctx, toks, 32);
    if (n <= 0) {
        replyYarnView(ctx, true);
        return;
    }

    char err[48];
    if (!ctx.mod->yarn().dal().appendWords(lobbsCtxUserUuid(ctx), ctx.session.isSysop, toks, n, err, sizeof(err))) {
        lobbsCommandReply(ctx, err[0] ? err : "Failed.");
        return;
    }
    replyYarnView(ctx, true);
}

static void slashYarn(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb || strcasecmp(verb, "yarn") != 0)
        return;
    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;
    handleYarn(c);
}

static void filterYarnRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    if (query)
        return;
    lobbsRootCommandPush(lines, "yarn");
}

static void filterYarnCommandHelp(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    lobbsCommandHelpPush(lines, "yarn", yarnHelp, sizeof(yarnHelp) / sizeof(yarnHelp[0]), query);
}

static void filterYarnStatusLines(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)query;
    if (!ctx || !ctx->mod)
        return;
    char line[64];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->yarn().dal().newWordsForUser(lobbsCtxUserUuid(*ctx));
        snprintf(line, sizeof(line), "Yarn: %u", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->yarn().dal().totalWordsAppended();
        snprintf(line, sizeof(line), "Yarn: %u (all time)", (unsigned)n);
    }
    lines.push_back(line);
}

void lobbsYarnRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashYarn, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("root_commands", filterYarnRootCommands, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("command_help", filterYarnCommandHelp, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterYarnStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
