#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSHistory.h"
#include "MailDal.h"
#include <stddef.h>
#include <stdint.h>

class MailApp
{
  public:
    explicit MailApp(LoDb &lodb);
    MailDal &dal() { return dal_; }

  private:
    MailDal dal_;
};

void lobbsMailPush(LobbsHistory *h);
void lobbsMailPushInbox(LobbsHistory *h, uint64_t inboxUuid);
void lobbsMailPushRead(LobbsHistory *h, uint64_t inboxUuid, uint32_t idx);
void lobbsMailItemStatus(LobbsHistory *h, char *buf, size_t cap);

#endif
