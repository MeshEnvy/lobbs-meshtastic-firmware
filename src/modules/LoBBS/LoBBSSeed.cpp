#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSConfig.h"
#if LOBBS_SEED

#include "LoBBSModule.h"
#include "LoBBSSeed.h"
#include "apps/Auth/AuthSeed.h"
#include "apps/Mail/MailSeed.h"
#include "apps/News/NewsSeed.h"
#include "apps/Wall/WallSeed.h"
#include "apps/Yarn/YarnSeed.h"

void lobbsSeedAll(LoBBSModule &mod)
{
    lobbsSeedAuth(mod);
    lobbsSeedMail(mod);
    lobbsSeedNews(mod);
    lobbsSeedWall(mod);
    lobbsSeedYarn(mod);
}

#endif
#endif
