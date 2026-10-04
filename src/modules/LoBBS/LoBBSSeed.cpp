#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSConfig.h"
#if LOBBS_SEED

#include "LoBBSCommandCtx.h"
#include "LoBBSHooks.h"
#include "LoBBSModule.h"
#include "LoBBSSeed.h"

void lobbsSeedAll(LoBBSModule &mod)
{
    LoBBSCommandCtx ctx;
    ctx.mod = &mod;
    lobbsDoAction("seed", ctx, LoScalar());
}

#endif
#endif
