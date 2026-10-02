#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../../LoBBSHistory.h"
#include "NewsDal.h"
#include <stddef.h>
#include <stdint.h>

class NewsApp
{
  public:
    explicit NewsApp(LoDb &lodb);
    NewsDal &dal() { return dal_; }

  private:
    NewsDal dal_;
};

void lobbsNewsPush(LobbsHistory *h);
void lobbsNewsPushList(LobbsHistory *h);
void lobbsNewsPushRead(LobbsHistory *h, uint32_t idx);
void lobbsNewsItemStatus(LobbsHistory *h, char *buf, size_t cap);

#endif
