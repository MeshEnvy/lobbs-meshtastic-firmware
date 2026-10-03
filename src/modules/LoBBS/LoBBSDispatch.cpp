#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDispatch.h"
#include "LoBBSCommandRegistry.h"
#include "LoBBSModule.h"
#include "apps/Auth/AuthDal.h"
#include "apps/Auth/auth.pb.h"
#include "mesh/NodeDB.h"
#include <cstring>

static uint32_t lobbsSessionNodeId(const meshtastic_MeshPacket &mp)
{
    return getFrom(&mp);
}

static void lobbsTrimLine(char *line)
{
    if (!line)
        return;
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' || line[len - 1] == '\r' || line[len - 1] == '\n'))
        line[--len] = '\0';
    char *start = line;
    while (*start == ' ' || *start == '\t')
        start++;
    if (start != line)
        memmove(line, start, strlen(start) + 1);
}

static bool lobbsIgnoreLoopbackPacket(const meshtastic_MeshPacket &mp)
{
    return mp.from == nodeDB->getNodeNum();
}

ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp)
{
    if (!isToUs(&mp))
        return ProcessMessage::CONTINUE;

    if (mp.decoded.payload.size == 0)
        return ProcessMessage::CONTINUE;

    if (lobbsIgnoreLoopbackPacket(mp))
        return ProcessMessage::CONTINUE;

    size_t copyLen = mp.decoded.payload.size;
    if (copyLen >= sizeof(mod->msgBuffer))
        copyLen = sizeof(mod->msgBuffer) - 1;
    memcpy(mod->msgBuffer, mp.decoded.payload.bytes, copyLen);
    mod->msgBuffer[copyLen] = '\0';

    lobbsTrimLine(mod->msgBuffer);
    if (mod->msgBuffer[0] == '\0' || mod->msgBuffer[0] != '/')
        return ProcessMessage::CONTINUE;

    const uint32_t sessionNodeId = lobbsSessionNodeId(mp);
    AuthDal &auth = mod->auth().dal();

    meshtastic_LoBBSUser existingUser = meshtastic_LoBBSUser_init_zero;
    bool isAuthenticated = auth.loadUserByNodeId(sessionNodeId, &existingUser);
    const bool isAdmin = isAuthenticated && existingUser.is_admin;
    const meshtastic_LoBBSUser *userPtr = isAuthenticated ? &existingUser : nullptr;

    lobbsCommandsHandle(mod, mp, sessionNodeId, isAuthenticated, userPtr, isAdmin, mod->msgBuffer);
    return ProcessMessage::CONTINUE;
}

#endif
