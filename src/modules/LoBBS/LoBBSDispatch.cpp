#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDispatch.h"
#include "LoBBSCommandRegistry.h"
#include "LoBBSModule.h"
#include "apps/Auth/AuthDal.h"
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

static LoBBSSession lobbsResolveSession(LoBBSModule *mod, uint32_t nodeId)
{
    LoBBSSession session;
    session.nodeId = nodeId;
    AuthDal &auth = mod->auth().dal();
    LoScalar userRow;
    if (!auth.loadUserByNodeId(nodeId, &userRow))
        return session;
    session.userUuid = AuthDal::userUuid(userRow);
    AuthDal::userUsername(userRow, session.username, sizeof(session.username));
    session.isSysop = AuthDal::userIsSysop(userRow);
    return session;
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

    const LoBBSSession session = lobbsResolveSession(mod, lobbsSessionNodeId(mp));
    lobbsCommandsHandle(mod, mp, session, mod->msgBuffer);
    return ProcessMessage::CONTINUE;
}

#endif
