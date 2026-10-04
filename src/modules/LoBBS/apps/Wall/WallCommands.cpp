#if !MESHTASTIC_EXCLUDE_LOBBS

#include "WallCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSResponse.h"
#include "WallDal.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <strings.h>

static const LoBBSSubHelpEntry wallHelp[] = {
    {"view", "view — /wall shows 12x12 grid (no login)"},
    {"paint", "paint — a4x set; -a4 blank; 1/cycle default"},
    {"limit", "limit — sysop: /wall limit SEC CELLS"},
};

static void replyGrid(LoBBSCommandCtx &ctx, bool markSeen)
{
    WallDal &wall = ctx.mod->wall().dal();
    char buf[LOBBS_REPLY_BYTES + 1];
    if (!wall.formatGridLines(buf, sizeof(buf))) {
        lobbsCommandReplyError(ctx, "Canvas error.");
        return;
    }
    if (markSeen && lobbsCtxLoggedIn(ctx))
        wall.markSeen(lobbsCtxUserUuid(ctx), wall.canvasCrc32());
    lobbsCommandReply(ctx, buf);
}

static void wallSubLimit(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    lobbsArgShift(ctx);
    uint32_t period = 0;
    uint32_t cells = 0;
    if (!lobbsArgShiftUint(ctx, period) || !lobbsArgShiftUint(ctx, cells) || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /wall limit SEC CELLS");
        return;
    }
    char err[48];
    WallDal &wall = ctx.mod->wall().dal();
    if (!wall.setConfig(period, cells, err, sizeof(err))) {
        lobbsCommandReplyError(ctx, err[0] ? err : "Failed.");
        return;
    }
    char reply[64];
    snprintf(reply, sizeof(reply), "Limit %us %u cells.", (unsigned)period, (unsigned)cells);
    lobbsCommandReply(ctx, reply);
}

static void handleWall(LoBBSCommandCtx &ctx)
{
    const char *peek = lobbsArgPeek(ctx);
    if (!peek) {
        replyGrid(ctx, true);
        return;
    }

    if (strcasecmp(peek, "limit") == 0) {
        wallSubLimit(ctx);
        return;
    }

    if (!lobbsCommandRequireLogin(ctx))
        return;

    const char *toks[48];
    int n = lobbsArgShiftMany(ctx, toks, 48);
    if (n <= 0) {
        replyGrid(ctx, true);
        return;
    }

    char err[48];
    if (!ctx.mod->wall().dal().applyPaintTokens(lobbsCtxUserUuid(ctx), ctx.session.isSysop, toks, n, err, sizeof(err))) {
        lobbsCommandReplyError(ctx, err[0] ? err : "Paint failed.");
        return;
    }
    replyGrid(ctx, true);
}

static void slashWall(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    if (!ctx || !lobbsSlashVerbIs(args, "wall"))
        return;
    handleWall(*ctx);
}

static void filterWallHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)ctx;
    (void)args;
    lobbsRecordPush(topics, "wall", "shared 12x12 paint grid");
}

static void filterWallHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    (void)ctx;
    lobbsHelpForTopic(value, args, "wall", wallHelp, sizeof(wallHelp) / sizeof(wallHelp[0]));
}

static void filterWallStatusLines(LoBBSCommandCtx *ctx, std::vector<LoScalar> &lines, const LoScalar &args)
{
    (void)args;
    if (!ctx || !ctx->mod || !lobbsCtxLoggedIn(*ctx))
        return;
    bool dirty = ctx->mod->wall().dal().isDirtyForUser(lobbsCtxUserUuid(*ctx));
    lobbsRecordPush(lines, "Wall", dirty ? "new" : "seen");
}

void lobbsWallRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashWall, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterWallHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterWallHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("status_lines", filterWallStatusLines, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
