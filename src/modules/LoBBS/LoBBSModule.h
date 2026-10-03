#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSVersion.h"
#include "MeshModule.h"
#include "SinglePortModule.h"
#include "apps/Auth/Auth.h"
#include "apps/Mail/Mail.h"
#include "apps/News/News.h"
#include <lodb/LoDB.h>
#include <string>

class LoBBSModule;

void lobbsBreadcrumb(const char *step);

class LoBBSModule : public SinglePortModule
{
    friend ProcessMessage lobbsDispatchReceived(class LoBBSModule *mod, const meshtastic_MeshPacket &mp);

  public:
    LoBBSModule();
    ~LoBBSModule();

    static constexpr size_t MAX_REPLY_BYTES = 200;
    void sendReply(const meshtastic_MeshPacket &req, const char *msg);
    void sendReply(const meshtastic_MeshPacket &req, const std::string &msg) { sendReply(req, msg.c_str()); }

    AuthApp &auth() { return auth_; }
    MailApp &mail() { return mail_; }
    NewsApp &news() { return news_; }

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    char msgBuffer[256];

  private:
    LoDb *lodb_;
    AuthApp auth_;
    MailApp mail_;
    NewsApp news_;
};

extern LoBBSModule *lobbsModule;

#endif
