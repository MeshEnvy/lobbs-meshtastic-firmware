#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../Auth/AuthDal.h"
#include "WallDal.h"
#include "WallSeed.h"

void lobbsSeedWall(LoBBSModule &mod)
{
    AuthDal &auth = mod.auth().dal();
    WallDal &wall = mod.wall().dal();
    uint64_t sysop = auth.getUserUuidByUsername("sysop");
    if (!sysop)
        return;
    const char *tokens[] = {"a1#", "a2#", "b2#", "c3x", "d4y"};
    char err[48];
    wall.applyPaintTokens(sysop, true, tokens, 5, err, sizeof(err));
}

#endif
#endif
