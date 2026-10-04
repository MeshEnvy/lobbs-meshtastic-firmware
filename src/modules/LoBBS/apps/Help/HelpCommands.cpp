#if !MESHTASTIC_EXCLUDE_LOBBS

#include "HelpCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSConfig.h"
#include "../../LoBBSReply.h"
#include <cstdio>
#include <cstring>

static void replyHelpRoot(LoBBSCommandCtx &ctx)
{
    LoBBSFilterLines index{};
    lobbsApplyFilters("help_index", &index, &ctx);

    char buf[LOBBS_REPLY_BYTES + 1];
    size_t n = 0;
    const char *header = "LoBBS Commands\n" LOBBS_HELP_HINT "\nAvailable commands: ";
    for (const char *s = header; *s && n + 1 < sizeof(buf); s++)
        buf[n++] = *s;
    bool first = true;
    for (int i = 0; i < index.count; i++) {
        if (!index.line[i][0])
            continue;
        if (!first && n + 2 < sizeof(buf)) {
            buf[n++] = ',';
            buf[n++] = ' ';
        }
        first = false;
        for (const char *s = index.line[i]; *s && n + 1 < sizeof(buf); s++)
            buf[n++] = *s;
    }
    buf[n] = '\0';
    lobbsCommandReply(ctx, buf);
}

static void handleHelp(LoBBSCommandCtx &ctx)
{
    (void)lobbsCommandTakePageArg(ctx);
    LoBBSFilterHelpTopics topics{};
    lobbsApplyFilters("help_topics", &topics, &ctx);

    const char *topic = ctx.argc >= 2 ? ctx.argv[1] : nullptr;
    if (!topic) {
        replyHelpRoot(ctx);
        return;
    }

    const char *verb = ctx.argc >= 3 ? ctx.argv[2] : nullptr;
    for (int i = 0; i < topics.count; i++) {
        const auto &slot = topics.slot[i];
        if (!slot.topic || strcasecmp(slot.topic, topic) != 0)
            continue;
        if (!slot.entries || slot.entryCount == 0) {
            lobbsCommandReply(ctx, "No help.");
            return;
        }
        lobbsCommandReplySubHelpTopic(ctx, slot.title, slot.entries, slot.entryCount, verb);
        return;
    }

    replyHelpRoot(ctx);
}

static void filterHelpCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterCommandsAdd(*(LoBBSFilterCommands *)value, "help", handleHelp);
}

void lobbsHelpRegisterCommands()
{
    LoBBSAppHooks hooks{};
    hooks.commands = filterHelpCommands;
    hooks.priority = LOBBS_FILTER_PRIORITY_HELP;
    lobbsAppRegisterHooks(hooks);
}

#endif
