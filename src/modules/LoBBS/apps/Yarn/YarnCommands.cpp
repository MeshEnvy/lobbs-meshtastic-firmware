#if !MESHTASTIC_EXCLUDE_LOBBS

#include "YarnCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "YarnDal.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
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
        lobbsCommandReplyError(ctx, "Yarn error.");
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
        lobbsCommandReplyError(ctx, "Usage: /yarn limit SEC WORDS CHARS");
        return;
    }
    char err[48];
    if (!ctx.mod->yarn().dal().setConfig(period, words, chars, err, sizeof(err))) {
        lobbsCommandReplyError(ctx, err[0] ? err : "Failed.");
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
        lobbsCommandReplyError(ctx, err[0] ? err : "Failed.");
        return;
    }
    replyYarnView(ctx, true);
}

static void slashYarn(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "yarn"))
        return;
    handleYarn(*ctx);
}

static void filterYarnHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsRecordPush(topics, "yarn", "shared story, a few words at a time");
}

static void filterYarnHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    (void)ctx;
    lobbsHelpForTopic(value, args, "yarn", yarnHelp, sizeof(yarnHelp) / sizeof(yarnHelp[0]));
}

static void filterYarnStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod)
        return;
    char value[32];
    if (lobbsCtxLoggedIn(*ctx)) {
        uint32_t n = ctx->mod->yarn().dal().newWordsForUser(lobbsCtxUserUuid(*ctx));
        snprintf(value, sizeof(value), "%u new words", (unsigned)n);
    } else {
        uint32_t n = ctx->mod->yarn().dal().totalWordsAppended();
        snprintf(value, sizeof(value), "%u words total", (unsigned)n);
    }
    lobbsRecordPush(lines, "Yarn", value);
}

#if LOBBS_SEED
#include "YarnSeed.h"
static void actionYarnSeed(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    (void)args;
    if (ctx && ctx->mod)
        lobbsSeedYarn(*ctx->mod);
}
#endif

void lobbsYarnRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashYarn, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterYarnHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterYarnHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterYarnStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
#if LOBBS_SEED
    lobbsAddAction("seed", actionYarnSeed, LOBBS_HOOK_PRIORITY_FEATURE);
#endif
}

#endif
