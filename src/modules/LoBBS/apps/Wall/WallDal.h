#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "wall.pb.h"
#include <lodb/LoDB.h>
#include <stddef.h>
#include <stdint.h>

static constexpr int LOBBS_WALL_ROWS = 12;
static constexpr int LOBBS_WALL_COLS = 12;
static constexpr int LOBBS_WALL_CELLS = LOBBS_WALL_ROWS * LOBBS_WALL_COLS;

static constexpr uint32_t LOBBS_WALL_DEFAULT_PERIOD_SEC = 3600;
static constexpr uint32_t LOBBS_WALL_DEFAULT_MAX_CELLS = 1;

class WallDal
{
  public:
    explicit WallDal(LoDb &lodb);

    bool formatGridLines(char *out, size_t outCap);
    uint32_t canvasCrc32();
    bool markSeen(uint64_t userUuid, uint32_t crc32);
    bool isDirtyForUser(uint64_t userUuid);
    bool applyPaintTokens(uint64_t userUuid, bool isSysop, const char *const *tokens, int count, char *err, size_t errCap);
    bool setConfig(uint32_t periodSeconds, uint32_t maxCellsPerCycle, char *err, size_t errCap);

  private:
    bool loadCanvas(meshtastic_LoBBSWallCanvas &out);
    bool saveCanvas(meshtastic_LoBBSWallCanvas &canvas);
    bool loadConfig(meshtastic_LoBBSWallConfig &out);
    bool saveQuota(meshtastic_LoBBSWallQuota &quota);
    bool checkPaintQuota(uint64_t userUuid, bool isSysop, int tokenCount, char *err, size_t errCap);
    bool recordPaintQuota(uint64_t userUuid, int tokenCount);
    uint32_t computeCrc32(const uint8_t *data, size_t len);
    uint32_t getLastSeenCrc(uint64_t userUuid);
    LoDb &lodb_;
};

#endif
