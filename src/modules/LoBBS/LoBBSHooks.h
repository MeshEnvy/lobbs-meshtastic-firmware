#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSCommandCtx.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

struct LoBBSSubHelpEntry {
    const char *verb;
    const char *line;
};

static constexpr int LOBBS_HOOK_PRIORITY_HELP = 0;
static constexpr int LOBBS_HOOK_PRIORITY_AUTH = 10;
static constexpr int LOBBS_HOOK_PRIORITY_FEATURE = 20;
static constexpr int LOBBS_HOOK_PRIORITY_STATUS = 30;
static constexpr int LOBBS_HOOK_PRIORITY_DEFAULT = 10;

using LoBBSActionFn = void (*)(LoBBSCommandCtx *ctx, const char *verb, const char *rest);
using LoBBSFilterFn = void (*)(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query);

void lobbsHooksReset();
void lobbsAddAction(const char *name, LoBBSActionFn fn, int priority = LOBBS_HOOK_PRIORITY_DEFAULT);
void lobbsAddFilter(const char *name, LoBBSFilterFn fn, int priority = LOBBS_HOOK_PRIORITY_DEFAULT);
void lobbsDoAction(const char *name, LoBBSCommandCtx &ctx, const char *verb, const char *rest);
void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, std::vector<std::string> &lines, const char *query = nullptr);

#endif
