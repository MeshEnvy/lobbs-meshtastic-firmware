#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "apps/Auth/auth.pb.h"
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

static constexpr int LOBBS_CMD_MAX_ARGC = 24;

struct LoBBSCommandCtx {
    LoBBSModule *mod = nullptr;
    const meshtastic_MeshPacket *mp = nullptr;
    uint32_t reqId = 0;
    uint32_t sessionNodeId = 0;
    bool isAuth = false;
    const meshtastic_LoBBSUser *user = nullptr;
    bool isSysop = false;
    uint32_t page = 1;
    int argc = 0;
    char *argv[LOBBS_CMD_MAX_ARGC];
};

#endif
