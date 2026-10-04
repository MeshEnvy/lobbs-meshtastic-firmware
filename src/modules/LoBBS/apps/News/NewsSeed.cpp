#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../Auth/AuthDal.h"
#include "NewsDal.h"
#include "NewsSeed.h"
#include <cstdio>

void lobbsSeedNews(LoBBSModule &mod)
{
    AuthDal &auth = mod.auth().dal();
    NewsDal &news = mod.news().dal();
    for (int i = 1; i <= 10; i++) {
        char authorName[16];
        snprintf(authorName, sizeof(authorName), "demo%02d", ((i - 1) % 12) + 1);
        uint64_t author = auth.getUserUuidByUsername(authorName);
        if (!author)
            continue;
        char body[80];
        snprintf(body, sizeof(body), "Demo news item %d for paging tests on the board.", i);
        news.postNews(author, body);
    }
}

#endif
#endif
