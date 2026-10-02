#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSHistory.h"
#include "AuthDal.h"

class AuthApp
{
  public:
    explicit AuthApp(LoDb &lodb);
    AuthDal &dal() { return dal_; }

  private:
    AuthDal dal_;
};

void lobbsAuthPush(LobbsHistory *h);

#endif
