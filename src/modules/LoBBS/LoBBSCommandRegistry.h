#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSHooks.h"
#include "LoBBSCommandCtx.h"
#include <stddef.h>
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

struct LoBBSSubcommand {
    const char *name;
    LoBBSCommandHandler handler;
};

void lobbsCommandReplySubHelpTopic(LoBBSCommandCtx &ctx, const char *title, const LoBBSSubHelpEntry *entries, size_t count,
                                   const char *verbOrNull);
bool lobbsCommandTrySubHelp(LoBBSCommandCtx &ctx, const char *title, const LoBBSSubHelpEntry *entries, size_t count);

void lobbsCommandReply(LoBBSCommandCtx &ctx, const char *body);
bool lobbsCtxLoggedIn(const LoBBSCommandCtx &ctx);
uint64_t lobbsCtxUserUuid(const LoBBSCommandCtx &ctx);
bool lobbsCtxUsername(const LoBBSCommandCtx &ctx, char *buf, size_t bufCap);
bool lobbsCommandRequireLogin(LoBBSCommandCtx &ctx);
bool lobbsCommandRequireSysop(LoBBSCommandCtx &ctx);
bool lobbsCommandNeedArgc(LoBBSCommandCtx &ctx, int min, const char *usage);
void lobbsCommandDispatchSub(LoBBSCommandCtx &ctx, const LoBBSSubcommand *subs, size_t count, const char *defaultName,
                             const char *unknownReply);
uint32_t lobbsCommandTakePageArg(LoBBSCommandCtx &ctx, uint32_t defaultPage = 1);
void lobbsCommandJoinArgs(const LoBBSCommandCtx &ctx, int from, int to, char *out, size_t outCap);
typedef void (*LoBBSPagerFormatLineFn)(void *ctx, uint32_t itemIndex, char *line, size_t lineCap);

bool lobbsPagerFormatLines(char *out, size_t outCap, uint32_t page1, const char *const *lines, uint32_t lineCount,
                           const char **errEmpty, const char **errBadPage);
bool lobbsPagerFormatItems(char *out, size_t outCap, uint32_t page1, uint32_t itemCount, LoBBSPagerFormatLineFn fn,
                           void *fnCtx, const char **errEmpty, const char **errBadPage);

void lobbsCommandsInstall(const LoBBSFilterCommands &cmds);
void lobbsCommandsHandle(LoBBSModule *mod, const meshtastic_MeshPacket &mp, const LoBBSSession &session, char *line);

#endif
