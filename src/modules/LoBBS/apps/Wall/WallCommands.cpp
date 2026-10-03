#if !MESHTASTIC_EXCLUDE_LOBBS

#include "WallCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "WallDal.h"
#include <cstdio>
#include <cstring>
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
        lobbsCommandReply(ctx, "Canvas error.");
        return;
    }
    if (markSeen && ctx.user)
        wall.markSeen(ctx.user->uuid, wall.canvasCrc32());
    lobbsCommandReply(ctx, buf);
}

static void wallSubLimit(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    if (!lobbsCommandNeedArgc(ctx, 4, "Usage: /wall limit SEC CELLS"))
        return;
    uint32_t period = (uint32_t)atoi(ctx.argv[2]);
    uint32_t cells = (uint32_t)atoi(ctx.argv[3]);
    char err[48];
    WallDal &wall = ctx.mod->wall().dal();
    if (!wall.setConfig(period, cells, err, sizeof(err))) {
        lobbsCommandReply(ctx, err[0] ? err : "Failed.");
        return;
    }
    char reply[64];
    snprintf(reply, sizeof(reply), "Limit %us %u cells.", (unsigned)period, (unsigned)cells);
    lobbsCommandReply(ctx, reply);
}

static void handleWall(LoBBSCommandCtx &ctx)
{
    if (lobbsCommandTrySubHelp(ctx, "Wall Help", wallHelp, sizeof(wallHelp) / sizeof(wallHelp[0])))
        return;

    if (ctx.argc < 2) {
        replyGrid(ctx, true);
        return;
    }

    if (ctx.argv[1] && strcasecmp(ctx.argv[1], "limit") == 0) {
        wallSubLimit(ctx);
        return;
    }

    if (!lobbsCommandRequireLogin(ctx))
        return;

    char err[48];
    if (!ctx.mod->wall().dal().applyPaintTokens(ctx.user->uuid, ctx.isSysop, (const char *const *)&ctx.argv[1],
                                                ctx.argc - 1, err, sizeof(err))) {
        lobbsCommandReply(ctx, err[0] ? err : "Paint failed.");
        return;
    }
    replyGrid(ctx, true);
}

static void filterWallCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterCommandsAdd(*(LoBBSFilterCommands *)value, "wall", handleWall);
}

static void filterWallStatusLines(void *value, LoBBSCommandCtx *ctx)
{
    if (!ctx || !ctx->mod || !ctx->isAuth || !ctx->user)
        return;
    char line[LOBBS_FILTER_LINE_BYTES];
    bool dirty = ctx->mod->wall().dal().isDirtyForUser(ctx->user->uuid);
    snprintf(line, sizeof(line), "Wall: %s", dirty ? "new" : "seen");
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, line);
}

static void filterWallHelpTopics(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterHelpTopicAdd(*(LoBBSFilterHelpTopics *)value, "wall", "Wall Help", wallHelp,
                            sizeof(wallHelp) / sizeof(wallHelp[0]));
}

static void filterWallHelpIndex(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, "wall");
}

void lobbsWallRegisterCommands()
{
    lobbsRegisterFilter("commands", filterWallCommands);
    lobbsRegisterFilter("status_lines", filterWallStatusLines);
    lobbsRegisterFilter("help_topics", filterWallHelpTopics);
    lobbsRegisterFilter("help_index", filterWallHelpIndex);
}

#endif
