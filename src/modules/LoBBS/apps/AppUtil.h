#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Auth/auth.pb.h"
#include <stddef.h>
#include <stdint.h>

class AuthDal;

void lobbsAppCopyCapped(char *dst, size_t dstCap, const char *src, size_t srcCap);
void lobbsAppTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize);
void lobbsAppTruncMsg(const char *message, char *buffer, size_t bufferSize, size_t maxLen);
bool lobbsAppLoadUser(AuthDal &auth, uint64_t uuid, meshtastic_LoBBSUser *outUser);

#endif
