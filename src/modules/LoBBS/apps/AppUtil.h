#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../LoBBSDb.h"
#include "../LoBBSHistory.h"
#include <stddef.h>
#include <stdint.h>

void lobbsAppCopyCapped(char *dst, size_t dstCap, const char *src, size_t srcCap);
void lobbsAppTimeAgo(uint32_t timestamp, char *buffer, size_t bufferSize);
void lobbsAppTruncMsg(const char *message, char *buffer, size_t bufferSize, size_t maxLen);
bool lobbsAppLoadUser(LoBBSDb *db, uint64_t uuid, meshtastic_LoBBSUser *outUser);

void lobbsAppStatusCount(char *buf, size_t cap, uint16_t n);
void lobbsAppAppendMenuLine(LobbsHistory *h, char *buf, size_t cap, size_t *n, uint8_t index, const LobbsItem *item);

void lobbsAppDrawTitled(LobbsHistory *h, const LobbsFrame *self, const char *title);
void lobbsAppDrawPrompt(LobbsHistory *h, const LobbsFrame *self);

#endif
