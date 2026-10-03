#if !MESHTASTIC_EXCLUDE_LOBBS

#include "StatusCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSVersion.h"
#include <cstdio>
#include <cstring>

static void handleHi(LoBBSCommandCtx &ctx)
{
    char buf[LOBBS_REPLY_BYTES + 1];
    if (ctx.isAuth && ctx.user) {
        snprintf(buf, sizeof(buf),
                 "LoBBS v%s\nWelcome back, %s!\nUse /help for general help\nUse /help <cmd> for help with a command\nUse /status to see what's happening",
                 LOBBS_VERSION_SHORT, ctx.user->username);
    } else {
        snprintf(buf, sizeof(buf),
                 "LoBBS v%s\nWelcome!\nUse /help for general help\nUse /help <cmd> for help with a command\nUse /status to see what's happening\nUse /login to sign in",
                 LOBBS_VERSION_SHORT);
    }
    lobbsCommandReply(ctx, buf);
}

static void handleStatus(LoBBSCommandCtx &ctx)
{
    LoBBSFilterLines lines{};
    lobbsApplyFilters("status_lines", &lines, &ctx);
    if (lines.count == 0) {
        lobbsCommandReply(ctx, "No status.");
        return;
    }
    char buf[LOBBS_REPLY_BYTES + 1];
    size_t n = 0;
    for (int i = 0; i < lines.count; i++) {
        if (i > 0 && n + 1 < sizeof(buf))
            buf[n++] = '\n';
        const char *s = lines.line[i];
        while (*s && n + 1 < sizeof(buf))
            buf[n++] = *s++;
    }
    buf[n] = '\0';
    lobbsCommandReply(ctx, buf);
}

static void filterStatusCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    auto *cmds = (LoBBSFilterCommands *)value;
    lobbsFilterCommandsAdd(*cmds, "status", handleStatus);
    lobbsFilterCommandsAdd(*cmds, "hi", handleHi);
}

void lobbsStatusRegisterCommands()
{
    lobbsRegisterFilter("commands", filterStatusCommands);
}

#endif
