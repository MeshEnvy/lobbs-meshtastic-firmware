#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSResponse.h"
#include <stdint.h>

#ifndef LOBBS_REPLY_CACHE_TTL_SEC
#define LOBBS_REPLY_CACHE_TTL_SEC 300
#endif

#ifndef LOBBS_REPLY_CACHE_MAX_BYTES
#define LOBBS_REPLY_CACHE_MAX_BYTES 8192
#endif

void lobbsReplyCacheGc(class LoBBSModule *mod, uint32_t nowSec);
bool lobbsReplyCacheStore(class LoBBSModule *mod, uint32_t sessionNodeId, const LoBBSResponse &resp);
bool lobbsReplyCacheLoad(class LoBBSModule *mod, uint32_t sessionNodeId, LoBBSResponse &out);

#endif
