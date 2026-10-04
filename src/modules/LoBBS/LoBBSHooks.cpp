#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSHooks.h"
#include "configuration.h"
#include <cstring>

enum class LobbsHookKind : uint8_t { Action, Record, List };

struct LobbsHookEntry {
    const char *name = nullptr;
    LobbsHookKind kind = LobbsHookKind::Action;
    void (*fn)() = nullptr;
    int priority = LOBBS_HOOK_PRIORITY_DEFAULT;
};

static std::vector<LobbsHookEntry> lobbsHooks;

void lobbsHooksReset()
{
    lobbsHooks.clear();
}

static void lobbsAddHook(const char *name, LobbsHookKind kind, void (*fn)(), int priority)
{
    if (!name || !fn)
        return;
    lobbsHooks.push_back({name, kind, fn, priority});
}

void lobbsAddAction(const char *name, LoBBSActionFn fn, int priority)
{
    lobbsAddHook(name, LobbsHookKind::Action, reinterpret_cast<void (*)()>(fn), priority);
}

void lobbsAddFilter(const char *name, LoBBSRecordFilterFn fn, int priority)
{
    lobbsAddHook(name, LobbsHookKind::Record, reinterpret_cast<void (*)()>(fn), priority);
}

void lobbsAddFilter(const char *name, LoBBSListFilterFn fn, int priority)
{
    lobbsAddHook(name, LobbsHookKind::List, reinterpret_cast<void (*)()>(fn), priority);
}

/** Matching hooks in priority order; logs and drops hooks registered with a different kind. */
static void lobbsSortHookIndices(const char *name, LobbsHookKind kind, std::vector<size_t> &indices)
{
    indices.clear();
    for (size_t i = 0; i < lobbsHooks.size(); i++) {
        if (!lobbsHooks[i].name || strcasecmp(lobbsHooks[i].name, name) != 0)
            continue;
        if (lobbsHooks[i].kind != kind) {
            LOG_ERROR("LoBBS hook %s: handler registered with the wrong kind", name);
            continue;
        }
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

void lobbsDoAction(const char *name, LoBBSCommandCtx &ctx, const LoScalar &args)
{
    if (!name)
        return;
    std::vector<size_t> order;
    lobbsSortHookIndices(name, LobbsHookKind::Action, order);
    for (size_t idx : order)
        reinterpret_cast<LoBBSActionFn>(lobbsHooks[idx].fn)(&ctx, args);
}

void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, LoScalar &value, const LoScalar &args)
{
    if (!name)
        return;
    std::vector<size_t> order;
    lobbsSortHookIndices(name, LobbsHookKind::Record, order);
    for (size_t idx : order)
        reinterpret_cast<LoBBSRecordFilterFn>(lobbsHooks[idx].fn)(&ctx, value, args);
}

void lobbsApplyFilter(const char *name, LoBBSCommandCtx &ctx, std::vector<LoScalar> &value, const LoScalar &args)
{
    if (!name)
        return;
    std::vector<size_t> order;
    lobbsSortHookIndices(name, LobbsHookKind::List, order);
    for (size_t idx : order)
        reinterpret_cast<LoBBSListFilterFn>(lobbsHooks[idx].fn)(&ctx, value, args);
}

#endif
