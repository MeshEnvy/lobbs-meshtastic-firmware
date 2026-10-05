#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../AppUtil.h"
#include "YarnDal.h"
#include "YarnSeed.h"

#include "LoBBSStackGuard.h"

void lobbsSeedYarn(LoBBSModule &mod)
{
    YarnDal &yarn = mod.yarn().dal();
    uint64_t sysop = lobbsAppUuidForUsername(&mod, "sysop");
    if (!sysop)
        return;
    const char *words1[] = {"Demo", "yarn", "seed", "line", "one."};
    const char *words2[] = {"Another", "short", "contribution."};
    yarn.appendWords(sysop, true, words1, 5);
    yarn.appendWords(sysop, true, words2, 3);
}

#endif
#endif
