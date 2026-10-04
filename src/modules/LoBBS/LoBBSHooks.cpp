#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSHooks.h"
#include <cstring>

struct LobbsFilterEntry {
    const char *name = nullptr;
    LoBBSFilterFn fn = nullptr;
    int priority = LOBBS_FILTER_PRIORITY_DEFAULT;
};

static LobbsFilterEntry lobbsFilterTable[LOBBS_FILTER_CALLBACKS_MAX];
static int lobbsFilterCount = 0;

void lobbsFiltersReset()
{
    lobbsFilterCount = 0;
}

bool lobbsRegisterFilter(const char *name, LoBBSFilterFn fn, int priority)
{
    if (!name || !fn || lobbsFilterCount >= LOBBS_FILTER_CALLBACKS_MAX)
        return false;
    lobbsFilterTable[lobbsFilterCount].name = name;
    lobbsFilterTable[lobbsFilterCount].fn = fn;
    lobbsFilterTable[lobbsFilterCount].priority = priority;
    lobbsFilterCount++;
    return true;
}

void lobbsAppRegisterHooks(const LoBBSAppHooks &hooks)
{
    if (hooks.commands)
        lobbsRegisterFilter("commands", hooks.commands, hooks.priority);
    if (hooks.status_lines)
        lobbsRegisterFilter("status_lines", hooks.status_lines, hooks.priority);
    if (hooks.help_topics)
        lobbsRegisterFilter("help_topics", hooks.help_topics, hooks.priority);
    if (hooks.help_index)
        lobbsRegisterFilter("help_index", hooks.help_index, hooks.priority);
}

void lobbsApplyFilters(const char *name, void *value, LoBBSCommandCtx *ctx)
{
    if (!name)
        return;
    int order[LOBBS_FILTER_CALLBACKS_MAX];
    int n = 0;
    for (int i = 0; i < lobbsFilterCount; i++) {
        if (lobbsFilterTable[i].name && lobbsFilterTable[i].fn &&
            strcasecmp(lobbsFilterTable[i].name, name) == 0) {
            order[n++] = i;
        }
    }
    for (int a = 0; a < n; a++) {
        for (int b = a + 1; b < n; b++) {
            int ia = order[a];
            int ib = order[b];
            bool ibFirst = lobbsFilterTable[ib].priority < lobbsFilterTable[ia].priority ||
                           (lobbsFilterTable[ib].priority == lobbsFilterTable[ia].priority && ib < ia);
            if (ibFirst) {
                int tmp = order[a];
                order[a] = order[b];
                order[b] = tmp;
            }
        }
    }
    for (int k = 0; k < n; k++)
        lobbsFilterTable[order[k]].fn(value, ctx);
}

bool lobbsFilterCommandsAdd(LoBBSFilterCommands &cmds, const char *name, LoBBSCommandHandler handler)
{
    if (!name || !handler || cmds.count >= LOBBS_FILTER_COMMANDS_MAX)
        return false;
    for (int i = 0; i < cmds.count; i++) {
        if (cmds.slot[i].name && strcasecmp(cmds.slot[i].name, name) == 0)
            return false;
    }
    cmds.slot[cmds.count].name = name;
    cmds.slot[cmds.count].handler = handler;
    cmds.count++;
    return true;
}

bool lobbsFilterLinesPush(LoBBSFilterLines &lines, const char *text)
{
    if (!text || !text[0] || lines.count >= LOBBS_FILTER_LINES_MAX)
        return false;
    strncpy(lines.line[lines.count], text, LOBBS_FILTER_LINE_BYTES - 1);
    lines.line[lines.count][LOBBS_FILTER_LINE_BYTES - 1] = '\0';
    lines.count++;
    return true;
}

bool lobbsFilterHelpTopicAdd(LoBBSFilterHelpTopics &topics, const char *topic, const char *title,
                              const LoBBSSubHelpEntry *entries, size_t entryCount)
{
    if (!topic || !topic[0] || topics.count >= LOBBS_FILTER_HELP_TOPICS_MAX)
        return false;
    for (int i = 0; i < topics.count; i++) {
        if (topics.slot[i].topic && strcasecmp(topics.slot[i].topic, topic) == 0)
            return false;
    }
    auto &s = topics.slot[topics.count];
    s.topic = topic;
    s.title = title ? title : topic;
    s.entries = entries;
    s.entryCount = entryCount;
    topics.count++;
    return true;
}

#endif
