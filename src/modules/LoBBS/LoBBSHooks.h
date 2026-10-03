#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSCommandCtx.h"
#include <stddef.h>
#include <stdint.h>

typedef void (*LoBBSCommandHandler)(LoBBSCommandCtx &ctx);
typedef void (*LoBBSFilterFn)(void *value, LoBBSCommandCtx *ctx);

static constexpr int LOBBS_FILTER_CALLBACKS_MAX = 24;
static constexpr int LOBBS_FILTER_COMMANDS_MAX = 16;
static constexpr int LOBBS_FILTER_LINES_MAX = 8;
static constexpr size_t LOBBS_FILTER_LINE_BYTES = 48;
static constexpr int LOBBS_FILTER_HELP_TOPICS_MAX = 12;

struct LoBBSSubHelpEntry {
    const char *verb;
    const char *line;
};

struct LoBBSFilterCommandSlot {
    const char *name = nullptr;
    LoBBSCommandHandler handler = nullptr;
};

struct LoBBSFilterCommands {
    LoBBSFilterCommandSlot slot[LOBBS_FILTER_COMMANDS_MAX];
    int count = 0;
};

struct LoBBSFilterLines {
    char line[LOBBS_FILTER_LINES_MAX][LOBBS_FILTER_LINE_BYTES];
    int count = 0;
};

struct LoBBSFilterHelpTopicSlot {
    const char *topic = nullptr;
    const char *title = nullptr;
    const LoBBSSubHelpEntry *entries = nullptr;
    size_t entryCount = 0;
};

struct LoBBSFilterHelpTopics {
    LoBBSFilterHelpTopicSlot slot[LOBBS_FILTER_HELP_TOPICS_MAX];
    int count = 0;
};

void lobbsFiltersReset();
bool lobbsRegisterFilter(const char *name, LoBBSFilterFn fn);
void lobbsApplyFilters(const char *name, void *value, LoBBSCommandCtx *ctx);

bool lobbsFilterCommandsAdd(LoBBSFilterCommands &cmds, const char *name, LoBBSCommandHandler handler);
bool lobbsFilterLinesPush(LoBBSFilterLines &lines, const char *text);
bool lobbsFilterHelpTopicAdd(LoBBSFilterHelpTopics &topics, const char *topic, const char *title,
                             const LoBBSSubHelpEntry *entries, size_t entryCount);

#endif
