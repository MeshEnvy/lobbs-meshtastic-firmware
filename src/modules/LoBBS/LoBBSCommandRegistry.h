#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSHooks.h"
#include "LoBBSCommandCtx.h"
#include <stddef.h>
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

void lobbsCommandReply(LoBBSCommandCtx &ctx, const char *body);
bool lobbsCtxLoggedIn(const LoBBSCommandCtx &ctx);
uint64_t lobbsCtxUserUuid(const LoBBSCommandCtx &ctx);
bool lobbsCtxUsername(const LoBBSCommandCtx &ctx, char *buf, size_t bufCap);
bool lobbsCommandRequireLogin(LoBBSCommandCtx &ctx);
bool lobbsCommandRequireSysop(LoBBSCommandCtx &ctx);

const char *lobbsArgShift(LoBBSCommandCtx &ctx);
const char *lobbsArgPeek(const LoBBSCommandCtx &ctx);
bool lobbsArgTakePage(LoBBSCommandCtx &ctx);
const char *lobbsArgRest(LoBBSCommandCtx &ctx);
bool lobbsArgHasMore(const LoBBSCommandCtx &ctx);
bool lobbsTokenIsPage(const char *tok);
bool lobbsTokIsUint(const char *tok, uint32_t &out);
bool lobbsArgPeekIsUint(const LoBBSCommandCtx &ctx);
bool lobbsArgPeekUint(const LoBBSCommandCtx &ctx, uint32_t &out);
bool lobbsArgShiftUint(LoBBSCommandCtx &ctx, uint32_t &out);

bool lobbsHelpQueryMatches(const char *query, const char *prefix);
void lobbsRootCommandPush(std::vector<std::string> &lines, const char *name);
void lobbsCommandHelpPush(std::vector<std::string> &lines, const char *topic, const LoBBSSubHelpEntry *entries,
                          size_t count, const char *query);
bool lobbsHelpStripTrailingPage(char *query, size_t queryCap, uint32_t &pageOut);
int lobbsArgShiftMany(LoBBSCommandCtx &ctx, const char *out[], int maxOut);
void lobbsCommandReplyPagedLines(LoBBSCommandCtx &ctx, const std::vector<std::string> &lines);

typedef void (*LoBBSPagerFormatLineFn)(void *ctx, uint32_t itemIndex, char *line, size_t lineCap);

bool lobbsPagerFormatLines(char *out, size_t outCap, uint32_t page1, const char *const *lines, uint32_t lineCount,
                           const char **errEmpty, const char **errBadPage);
bool lobbsPagerFormatItems(char *out, size_t outCap, uint32_t page1, uint32_t itemCount, LoBBSPagerFormatLineFn fn,
                           void *fnCtx, const char **errEmpty, const char **errBadPage);

void lobbsCommandsHandle(LoBBSModule *mod, const meshtastic_MeshPacket &mp, const LoBBSSession &session, char *line);

#endif
