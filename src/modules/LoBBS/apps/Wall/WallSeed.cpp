#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../AppUtil.h"
#include "WallDal.h"
#include "WallSeed.h"

void lobbsSeedWall(LoBBSModule &mod)
{
    WallDal &wall = mod.wall().dal();
    uint64_t sysop = lobbsAppUuidForUsername(&mod, "sysop");
    if (!sysop)
        return;
    const char *tokens[] = {"a1#", "a2#", "b2#", "c3x", "d4y"};
    wall.applyPaintTokens(sysop, true, tokens, 5);
}

#endif
#endif
