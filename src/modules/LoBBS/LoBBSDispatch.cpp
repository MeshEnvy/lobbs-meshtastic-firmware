#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDispatch.h"
#include "LoBBSCommandRegistry.h"
#include "LoBBSModule.h"
#include "apps/Auth/AuthDal.h"
#include "mesh/MeshTypes.h"
#include "mesh/NodeDB.h"
#include <cctype>
#include <cstring>

// Meshtastic phone ToRadio uses from=0; getFrom() maps that to our node num for session keys.
static uint32_t lobbsSessionKeyFromPacket(const meshtastic_MeshPacket &mp)
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

static bool lobbsLooksLikeEchoedReply(const char *text)
{
    if (!text || !text[0])
        return false;
    if (strncmp(text, "LoBBS v", 7) == 0)
        return true;
    if (strncmp(text, "Welcome ", 8) == 0)
        return true;
    if (strcmp(text, "Login required.") == 0)
        return true;
    if (strncmp(text, "Users:", 6) == 0)
        return true;
    return false;
}

static bool lobbsIgnoreLoopbackPacket(const meshtastic_MeshPacket &mp, const char *text)
{
    const uint32_t ourNode = nodeDB->getNodeNum();
    const bool isSlash = text && text[0] == '/';
    // Ignore mesh loopback of our replies; still accept /commands (including from=ourNode).
    if (mp.from == ourNode && !isSlash)
        return true;
    if (isFromUs(&mp) && lobbsLooksLikeEchoedReply(text))
        return true;
    return false;
}

static LoBBSSession lobbsResolveSession(LoBBSModule *mod, uint32_t wireNodeId)
{
    LoBBSSession session;
    session.nodeId = wireNodeId;
    AuthDal &auth = mod->auth().dal();
    LoScalar userRow;
    uint32_t sessionNodeId = wireNodeId;
    uint64_t authUserUuid = 0;
    if (!auth.loadUserByNodeId(wireNodeId, &userRow, &sessionNodeId, &authUserUuid))
        return session;
    session.nodeId = sessionNodeId;
    session.userUuid = authUserUuid;
    if (!AuthDal::userUsername(userRow, session.username, sizeof(session.username)))
        session.username[0] = '\0';
    session.isSysop = AuthDal::userIsSysop(userRow);
    return session;
}

ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp)
{
    if (!isToUs(&mp))
        return ProcessMessage::CONTINUE;

    if (mp.decoded.payload.size == 0)
        return ProcessMessage::CONTINUE;

    size_t copyLen = mp.decoded.payload.size;
    if (copyLen >= sizeof(mod->msgBuffer))
        copyLen = sizeof(mod->msgBuffer) - 1;
    memcpy(mod->msgBuffer, mp.decoded.payload.bytes, copyLen);
    mod->msgBuffer[copyLen] = '\0';

    lobbsTrimLine(mod->msgBuffer);
    if (lobbsIgnoreLoopbackPacket(mp, mod->msgBuffer))
        return ProcessMessage::CONTINUE;
    if (mod->msgBuffer[0] == '\0' || mod->msgBuffer[0] != '/')
        return ProcessMessage::CONTINUE;

    const LoBBSSession session = lobbsResolveSession(mod, lobbsSessionKeyFromPacket(mp));
    lobbsCommandsHandle(mod, mp, session, mod->msgBuffer);
    return ProcessMessage::CONTINUE;
}

#endif
