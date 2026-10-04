#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSConfig.h"
#if LOBBS_SEED

#include "../../LoBBSModule.h"
#include "../Auth/AuthDal.h"
#include "MailDal.h"
#include "MailSeed.h"
#include <cstdio>
#include <vector>

void lobbsSeedMail(LoBBSModule &mod)
{
    AuthDal &auth = mod.auth().dal();
    MailDal &mail = mod.mail().dal();
    uint64_t sysop = auth.getUserUuidByUsername("sysop");
    if (!sysop)
        return;

    for (int i = 1; i <= 8; i++) {
        char fromName[16];
        snprintf(fromName, sizeof(fromName), "demo%02d", i);
        uint64_t fromUuid = auth.getUserUuidByUsername(fromName);
        if (!fromUuid)
            continue;
        char body[64];
        snprintf(body, sizeof(body), "Demo mail #%d to sysop with extra text.", i);
        mail.sendMail(fromUuid, sysop, body);
    }

    std::vector<LoScalar> inbox = mail.getMailForUser(sysop, 0, 8);
    for (size_t j = 0; j < inbox.size() && j < 3; j++)
        mail.markMailAsRead(MailDal::mailUuid(inbox[j]));
}

#endif
#endif
