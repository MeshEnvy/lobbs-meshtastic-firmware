#if !MESHTASTIC_EXCLUDE_LOBBS

#include "TimeCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSReply.h"
#include "gps/RTC.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <sys/time.h>

static constexpr int LOBBS_HOOK_PRIORITY_TIME = 31;

static void handleTime(LoBBSCommandCtx &ctx)
{
    const char *peek = lobbsArgPeek(ctx);
    if (!peek) {
        char buf[LOBBS_REPLY_BYTES + 1];
        snprintf(buf, sizeof(buf), "Time: %u (%s)", (unsigned)getTime(), RtcName(getRTCQuality()));
        lobbsCommandReply(ctx, buf);
        return;
    }
    if (!lobbsCommandRequireSysop(ctx))
        return;
    uint32_t sec = 0;
    if (lobbsArgHasMore(ctx)) {
        lobbsCommandReply(ctx, "Usage: /time\nUsage: /time unix (sysop)");
        return;
    }
    if (!lobbsArgShiftUint(ctx, sec)) {
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
}

static const LoBBSSubHelpEntry timeHelp[] = {
    {"time", "time — show Unix time and source"},
    {"time", "time unix — sysop: set clock to Unix epoch"},
};

static void slashTime(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb || strcasecmp(verb, "time") != 0)
        return;
    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;
    handleTime(c);
}

static void filterTimeRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    if (query)
        return;
    lobbsRootCommandPush(lines, "time");
}

static void filterTimeCommandHelp(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    lobbsCommandHelpPush(lines, "time", timeHelp, sizeof(timeHelp) / sizeof(timeHelp[0]), query);
}

void lobbsTimeRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashTime, LOBBS_HOOK_PRIORITY_TIME);
    lobbsAddFilter("root_commands", filterTimeRootCommands, LOBBS_HOOK_PRIORITY_TIME);
    lobbsAddFilter("command_help", filterTimeCommandHelp, LOBBS_HOOK_PRIORITY_TIME);
}

#endif
