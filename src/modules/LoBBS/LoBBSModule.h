#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDal.h"
#include "LoBBSMenu.h"
#include "LoBBSVersion.h"
#include "SinglePortModule.h"
#include <string>

ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp);

// Direct USB line, then flush. The normal logger allocates, so a heap fault would hide the marker.
void lobbsBreadcrumb(const char *step);

class LoBBSModule : public SinglePortModule
{
    friend ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp);
    friend LobbsMenuKeyResult lobbsMenuTryGlobalKeys(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                                                     LoBBSDal *dal, bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin,
                                                     const char *line);
    friend void lobbsMenuHandleLine(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                                    bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin, const char *line);
    friend void lobbsMenuReprint(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                                 bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin);

  public:
    LoBBSModule();
    static constexpr size_t MAX_REPLY_BYTES = 200;
    void sendReply(const meshtastic_MeshPacket &req, const char *msg);
    void sendReply(const meshtastic_MeshPacket &req, const std::string &msg) { sendReply(req, msg.c_str()); }
    void sendPagedReply(uint32_t sessionNodeId, const meshtastic_MeshPacket &req, const char *body);

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    char msgBuffer[256];
    char replyBuffer[256];

  private:
    LoBBSDal *dal;
};

extern LoBBSModule *lobbsModule;

#endif
