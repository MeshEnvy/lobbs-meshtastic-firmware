#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSConfig.h"
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

static constexpr int LOBBS_CMD_MAX_ARGC = 24;

/** Resolved once per incoming command in dispatch (Auth owns load rules). */
struct LoBBSSession {
    uint32_t nodeId = 0;
    uint64_t userUuid = 0;
    char username[LOBBS_USERNAME_BUFFER_SIZE] = {0};
    bool isSysop = false;
};

struct LoBBSCommandCtx {
    LoBBSModule *mod = nullptr;
    const meshtastic_MeshPacket *mp = nullptr;
    uint32_t reqId = 0;
    LoBBSSession session;
    uint32_t page = 1;
    int argc = 0;
    char *argv[LOBBS_CMD_MAX_ARGC];
};

#endif
