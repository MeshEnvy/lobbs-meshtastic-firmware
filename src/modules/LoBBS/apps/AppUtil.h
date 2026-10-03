#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <loscalar/LoScalar.h>
#include <stddef.h>
#include <stdint.h>

class LoBBSModule;
struct LoBBSCommandCtx;

void lobbsAppCopyCapped(char *dst, size_t dstCap, const char *src, size_t srcCap);
void lobbsAppTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize);
void lobbsAppTruncMsg(const char *message, char *buffer, size_t bufferSize, size_t maxLen);

bool lobbsAppLoadUser(LoBBSModule *mod, uint64_t uuid, LoScalar *outUser);
/** Username for display; uses "unknown" when missing. */
void lobbsAppUsernameForUuid(LoBBSModule *mod, uint64_t uuid, char *buf, size_t bufCap);
uint64_t lobbsAppUuidForUsername(LoBBSModule *mod, const char *username);
/** Sets uuidOut and returns true, or replies and returns false. */
bool lobbsAppResolveUsername(LoBBSCommandCtx &ctx, const char *username, uint64_t &uuidOut);

#endif
