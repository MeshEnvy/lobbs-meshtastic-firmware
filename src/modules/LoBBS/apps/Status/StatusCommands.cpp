#if !MESHTASTIC_EXCLUDE_LOBBS

#include "StatusCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSReply.h"

static void handleStatus(LoBBSCommandCtx &ctx)
{
    lobbsArgTakePage(ctx);
    std::vector<std::string> lines;
    lobbsApplyFilter("status_lines", ctx, lines, nullptr);
    if (lines.empty()) {
        lobbsCommandReply(ctx, "No status.");
        return;
    }
    lobbsCommandReplyPagedLines(ctx, lines);
}

static void slashStatus(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb || strcasecmp(verb, "status") != 0)
        return;
    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;
    handleStatus(c);
}

static void filterStatusRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    if (query)
        return;
    lobbsRootCommandPush(lines, "status");
}

void lobbsStatusRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashStatus, LOBBS_HOOK_PRIORITY_STATUS);
    lobbsAddFilter("root_commands", filterStatusRootCommands, LOBBS_HOOK_PRIORITY_STATUS);
}

#endif
