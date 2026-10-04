#if !MESHTASTIC_EXCLUDE_LOBBS

#include "StatusCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSVersion.h"
#include "gps/RTC.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <sys/time.h>

static void handleHi(LoBBSCommandCtx &ctx)
{
    char buf[LOBBS_REPLY_BYTES + 1];
    if (lobbsCtxLoggedIn(ctx)) {
        char uname[LOBBS_USERNAME_BUFFER_SIZE];
        lobbsCtxUsername(ctx, uname, sizeof(uname));
        snprintf(buf, sizeof(buf),
                 "LoBBS v%s\nWelcome back, %s!\nUse /help [topic] for general help\nUse /status to see what's happening",
                 LOBBS_VERSION_SHORT, uname);
    } else {
        snprintf(buf, sizeof(buf),
                 "LoBBS v%s\nWelcome!\nUse /help [topic] for general help\nUse /status to see what's happening\nUse /login to sign in",
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

static void handleTime(LoBBSCommandCtx &ctx)
{
    if (ctx.argc == 1) {
        char buf[LOBBS_REPLY_BYTES + 1];
        snprintf(buf, sizeof(buf), "Time: %u (%s)", (unsigned)getTime(), RtcName(getRTCQuality()));
        lobbsCommandReply(ctx, buf);
        return;
    }
    if (ctx.argc == 2) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        char *end = nullptr;
        unsigned long sec = strtoul(ctx.argv[1], &end, 10);
        if (!ctx.argv[1][0] || end == ctx.argv[1] || *end != '\0' || sec > UINT32_MAX) {
            lobbsCommandReply(ctx, "Invalid time.");
            return;
        }
        struct timeval tv;
        tv.tv_sec = (time_t)sec;
        tv.tv_usec = 0;
        RTCSetResult r = perhapsSetRTC(RTCQualityNTP, &tv, true);
        if (r == RTCSetResultSuccess)
            lobbsCommandReply(ctx, "Time set.");
        else if (r == RTCSetResultInvalidTime)
            lobbsCommandReply(ctx, "Invalid time.");
        else
            lobbsCommandReply(ctx, "Could not set time.");
        return;
    }
    lobbsCommandReply(ctx, "Usage: /time\nUsage: /time unix (sysop)");
}

static const LoBBSSubHelpEntry timeHelp[] = {
    {"time", "time — show Unix time and source"},
    {"time", "time unix — sysop: set clock to Unix epoch"},
};

static void filterStatusCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    auto *cmds = (LoBBSFilterCommands *)value;
    lobbsFilterCommandsAdd(*cmds, "status", handleStatus);
    lobbsFilterCommandsAdd(*cmds, "hi", handleHi);
    lobbsFilterCommandsAdd(*cmds, "time", handleTime);
}

static void filterStatusHelpTopics(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterHelpTopicAdd(*(LoBBSFilterHelpTopics *)value, "time", "Time Help", timeHelp,
                            sizeof(timeHelp) / sizeof(timeHelp[0]));
}

static void filterStatusHelpIndex(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, "time");
}

void lobbsStatusRegisterCommands()
{
    LoBBSAppHooks hooks{};
    hooks.commands = filterStatusCommands;
    hooks.help_topics = filterStatusHelpTopics;
    hooks.help_index = filterStatusHelpIndex;
    hooks.priority = LOBBS_FILTER_PRIORITY_STATUS;
    lobbsAppRegisterHooks(hooks);
}

#endif
