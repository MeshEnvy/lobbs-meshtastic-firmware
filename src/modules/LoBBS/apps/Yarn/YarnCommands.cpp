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
    if (!lobbsCommandNeedArgc(ctx, 5, "Usage: /yarn limit SEC WORDS CHARS"))
        return;
    uint32_t period = (uint32_t)atoi(ctx.argv[2]);
    uint32_t words = (uint32_t)atoi(ctx.argv[3]);
    uint32_t chars = (uint32_t)atoi(ctx.argv[4]);
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
    if (lobbsCommandTrySubHelp(ctx, "Yarn Help", yarnHelp, sizeof(yarnHelp) / sizeof(yarnHelp[0])))
        return;

    if (ctx.argc >= 2 && strcasecmp(ctx.argv[1], "limit") == 0) {
        yarnSubLimit(ctx);
        return;
    }

    if (ctx.argc < 2) {
        replyYarnView(ctx, true);
        return;
    }

    if (!lobbsCommandRequireLogin(ctx))
        return;

    char err[48];
    if (!ctx.mod->yarn().dal().appendWords(lobbsCtxUserUuid(ctx), ctx.session.isSysop, (const char *const *)&ctx.argv[1],
                                           ctx.argc - 1, err, sizeof(err))) {
        lobbsCommandReply(ctx, err[0] ? err : "Failed.");
        return;
    }
    replyYarnView(ctx, true);
}

static void filterYarnCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterCommandsAdd(*(LoBBSFilterCommands *)value, "yarn", handleYarn);
}

static void filterYarnStatusLines(void *value, LoBBSCommandCtx *ctx)
{
    if (!ctx || !ctx->mod)
        return;
    char line[LOBBS_FILTER_LINE_BYTES];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->yarn().dal().newWordsForUser(lobbsCtxUserUuid(*ctx));
        snprintf(line, sizeof(line), "Yarn: %u", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->yarn().dal().totalWordsAppended();
        snprintf(line, sizeof(line), "Yarn: %u (all time)", (unsigned)n);
    }
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, line);
}

static void filterYarnHelpTopics(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterHelpTopicAdd(*(LoBBSFilterHelpTopics *)value, "yarn", "Yarn Help", yarnHelp,
                            sizeof(yarnHelp) / sizeof(yarnHelp[0]));
}

static void filterYarnHelpIndex(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, "yarn");
}

void lobbsYarnRegisterCommands()
{
    LoBBSAppHooks hooks{};
    hooks.commands = filterYarnCommands;
    hooks.status_lines = filterYarnStatusLines;
    hooks.help_topics = filterYarnHelpTopics;
    hooks.help_index = filterYarnHelpIndex;
    hooks.priority = LOBBS_FILTER_PRIORITY_FEATURE;
    lobbsAppRegisterHooks(hooks);
}

#endif
