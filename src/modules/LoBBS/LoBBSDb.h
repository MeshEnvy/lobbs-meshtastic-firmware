#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <lodb/LoDB.h>
#include <stdint.h>

#define LOBBS_MAX_USERNAME_LEN 32
#define LOBBS_USERNAME_BUFFER_SIZE (LOBBS_MAX_USERNAME_LEN + 1)
#define LOBBS_XSTR(x) LOBBS_STR(x)
#define LOBBS_STR(x) #x

class LoBBSDb
{
  public:
    explicit LoBBSDb(uint32_t hostNodeId);
    ~LoBBSDb();

    LoDb *getDb() { return db_; }
    uint32_t hostNodeId() const { return hostNodeId_; }

  private:
    LoDb *db_;
    uint32_t hostNodeId_;
};

#endif
