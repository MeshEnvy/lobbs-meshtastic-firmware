#if !MESHTASTIC_EXCLUDE_LOBBS

#include "HelpCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include <cstring>

static void lobbsTrimRestInPlace(char *s)
{
    if (!s)
        return;
    char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'))
        s[--len] = '\0';
}

static void replyCatalog(LoBBSCommandCtx &ctx, uint32_t page)
{
    ctx.page = page;
    std::vector<std::string> lines;
    lobbsApplyFilter("root_commands", ctx, lines, nullptr);
    lobbsCommandReplyPagedLines(ctx, lines);
}

static void replyTopicHelp(LoBBSCommandCtx &ctx, char *queryWork)
{
    lobbsTrimRestInPlace(queryWork);
    uint32_t page = ctx.page;
    if (lobbsHelpStripTrailingPage(queryWork, 128, page))
        ctx.page = page;

    std::vector<std::string> lines;
    const char *q = queryWork[0] ? queryWork : nullptr;
    lobbsApplyFilter("command_help", ctx, lines, q);
    if (lines.empty()) {
        lobbsCommandReply(ctx, "No help for that.");
        return;
    }
    lobbsCommandReplyPagedLines(ctx, lines);
}

static void slashHelp(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb)
        return;
    if (strcasecmp(verb, "help") != 0 && strcasecmp(verb, "hi") != 0)
        return;

    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;

    char work[128];
    work[0] = '\0';
    if (rest && rest[0])
        strncpy(work, rest, sizeof(work) - 1);

    lobbsTrimRestInPlace(work);
    if (!work[0]) {
        replyCatalog(c, 1);
        return;
    }

    char onlyPage[128];
    strncpy(onlyPage, work, sizeof(onlyPage) - 1);
    uint32_t page = 1;
    if (lobbsHelpStripTrailingPage(onlyPage, sizeof(onlyPage), page) && !onlyPage[0]) {
        replyCatalog(c, page);
        return;
    }

    replyTopicHelp(c, work);
}

static void filterHelpRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    if (query)
        return;
    lobbsRootCommandPush(lines, "help");
    lobbsRootCommandPush(lines, "hi");
}

void lobbsHelpRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashHelp, LOBBS_HOOK_PRIORITY_HELP);
    lobbsAddFilter("root_commands", filterHelpRootCommands, LOBBS_HOOK_PRIORITY_HELP);
}

#endif
