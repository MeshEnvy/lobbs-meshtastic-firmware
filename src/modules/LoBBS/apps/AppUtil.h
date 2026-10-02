#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../LoBBSDal.h"
#include "../LoBBSHistory.h"
#include <stddef.h>
#include <stdint.h>

void lobbsAppCopyCapped(char *dst, size_t dstCap, const char *src, size_t srcCap);
void lobbsAppTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize);
void lobbsAppTruncMsg(const char *message, char *buffer, size_t bufferSize, size_t maxLen);
bool lobbsAppLoadUser(LoBBSDal *dal, uint64_t uuid, meshtastic_LoBBSUser *outUser);

void lobbsAppDrawTitled(LobbsHistory *h, const LobbsFrame *self, const char *title);
void lobbsAppDrawPrompt(LobbsHistory *h, const LobbsFrame *self);

#endif
