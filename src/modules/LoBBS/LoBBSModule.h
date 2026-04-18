#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDal.h"
#include "LoBBSVersion.h"
#include "SinglePortModule.h"

class LoBBSModule : public SinglePortModule
{
  public:
    LoBBSModule();
    static constexpr size_t MAX_REPLY_BYTES = 200;

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    char msgBuffer[256];
    char replyBuffer[256];

  private:
    LoBBSDal *dal;
    void sendReply(NodeNum to, const std::string &msg);
};

extern LoBBSModule *lobbsModule;

#endif
