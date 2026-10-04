#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSHooks.h"
#include <cstring>

struct LobbsHookEntry {
    const char *name = nullptr;
    LoBBSActionFn action = nullptr;
    LoBBSFilterFn filter = nullptr;
    int priority = LOBBS_HOOK_PRIORITY_DEFAULT;
};

static std::vector<LobbsHookEntry> lobbsHooks;

void lobbsHooksReset()
{
    lobbsHooks.clear();
}

void lobbsAddAction(const char *name, LoBBSActionFn fn, int priority)
{
    if (!name || !fn)
        return;
    lobbsHooks.push_back({name, fn, nullptr, priority});
}

void lobbsAddFilter(const char *name, LoBBSFilterFn fn, int priority)
{
    if (!name || !fn)
        return;
    lobbsHooks.push_back({name, nullptr, fn, priority});
}

static void lobbsSortHookIndices(const char *name, std::vector<size_t> &indices)
{
    indices.clear();
    for (size_t i = 0; i < lobbsHooks.size(); i++) {
        if (lobbsHooks[i].name && strcasecmp(lobbsHooks[i].name, name) == 0)
            indices.push_back(i);
    }
    for (size_t a = 0; a < indices.size(); a++) {
        for (size_t b = a + 1; b < indices.size(); b++) {
            size_t ia = indices[a];
            size_t ib = indices[b];
            bool ibFirst = lobbsHooks[ib].priority < lobbsHooks[ia].priority ||
                           (lobbsHooks[ib].priority == lobbsHooks[ia].priority && ib < ia);
            if (ibFirst) {
                size_t tmp = indices[a];
                indices[a] = indices[b];
                indices[b] = tmp;
            }
        }
    }
}

void lobbsDoAction(const char *name, LoBBSCommandCtx &ctx, const char *verb, const char *rest)
{
    if (!name)
        return;
    std::vector<size_t> order;
    lobbsSortHookIndices(name, order);
    for (size_t idx : order) {
        if (lobbsHooks[idx].action)
            lobbsHooks[idx].action(&ctx, verb, rest);
    }
}

void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, std::vector<std::string> &lines, const char *query)
{
    if (!name)
        return;
    std::vector<size_t> order;
    lobbsSortHookIndices(name, order);
    for (size_t idx : order) {
        if (lobbsHooks[idx].filter)
            lobbsHooks[idx].filter(&ctx, lines, query);
    }
}

#endif
