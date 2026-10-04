#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../Auth/AuthDal.h"
#include "YarnDal.h"
#include "YarnSeed.h"

void lobbsSeedYarn(LoBBSModule &mod)
{
    AuthDal &auth = mod.auth().dal();
    YarnDal &yarn = mod.yarn().dal();
    uint64_t sysop = auth.getUserUuidByUsername("sysop");
    if (!sysop)
        return;
    const char *words1[] = {"Demo", "yarn", "seed", "line", "one."};
    const char *words2[] = {"Another", "short", "contribution."};
    char err[48];
    yarn.appendWords(sysop, true, words1, 5, err, sizeof(err));
    yarn.appendWords(sysop, true, words2, 3, err, sizeof(err));
}

#endif
#endif
