#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../LoBBSHistory.h"
#include <stdint.h>

void lobbsMailPush(LobbsHistory *h);
void lobbsMailPushInbox(LobbsHistory *h, uint64_t inboxUuid);
void lobbsMailPushRead(LobbsHistory *h, uint64_t inboxUuid, uint32_t idx);

#endif
