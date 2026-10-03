#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSHooks.h"
#include <cstring>

struct LobbsFilterEntry {
    const char *name = nullptr;
    LoBBSFilterFn fn = nullptr;
};

static LobbsFilterEntry lobbsFilterTable[LOBBS_FILTER_CALLBACKS_MAX];
static int lobbsFilterCount = 0;

void lobbsFiltersReset()
{
    lobbsFilterCount = 0;
}

bool lobbsRegisterFilter(const char *name, LoBBSFilterFn fn)
{
    if (!name || !fn || lobbsFilterCount >= LOBBS_FILTER_CALLBACKS_MAX)
        return false;
    lobbsFilterTable[lobbsFilterCount].name = name;
    lobbsFilterTable[lobbsFilterCount].fn = fn;
    lobbsFilterCount++;
    return true;
}

void lobbsApplyFilters(const char *name, void *value, LoBBSCommandCtx *ctx)
{
    if (!name)
        return;
    for (int i = 0; i < lobbsFilterCount; i++) {
        if (lobbsFilterTable[i].name && lobbsFilterTable[i].fn &&
            strcasecmp(lobbsFilterTable[i].name, name) == 0) {
            lobbsFilterTable[i].fn(value, ctx);
        }
    }
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
