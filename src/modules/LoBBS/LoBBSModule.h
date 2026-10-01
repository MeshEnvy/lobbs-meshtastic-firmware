#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDal.h"
#include "LoBBSVersion.h"
#include "SinglePortModule.h"

class LoBBSModule;

ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp);

class LoBBSModule : public SinglePortModule
{
    friend ProcessMessage lobbsDispatchReceived(LoBBSModule *mod, const meshtastic_MeshPacket &mp);

  public:
    LoBBSModule();
    static constexpr size_t MAX_REPLY_BYTES = 200;

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    char msgBuffer[256];
    char replyBuffer[256];

  private:
    LoBBSDal *dal;
    void sendReply(const meshtastic_MeshPacket &req, const std::string &msg);
    void sendPagedReply(uint32_t sessionNodeId, const meshtastic_MeshPacket &req, const std::string &body);
};

extern LoBBSModule *lobbsModule;

#endif
