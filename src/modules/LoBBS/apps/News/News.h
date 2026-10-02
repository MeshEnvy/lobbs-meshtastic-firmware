#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSHistory.h"
#include <stddef.h>
#include <stdint.h>

void lobbsNewsPush(LobbsHistory *h);
void lobbsNewsPushList(LobbsHistory *h);
void lobbsNewsPushRead(LobbsHistory *h, uint32_t idx);
void lobbsNewsItemStatus(LobbsHistory *h, char *buf, size_t cap);

#endif
