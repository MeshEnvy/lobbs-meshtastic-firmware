#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "yarn.pb.h"
#include <lodb/LoDB.h>
#include <stddef.h>
#include <stdint.h>

static constexpr size_t LOBBS_YARN_BODY_MAX = 186;

static constexpr uint32_t LOBBS_YARN_DEFAULT_PERIOD_SEC = 3600;
static constexpr uint32_t LOBBS_YARN_DEFAULT_MAX_WORDS = 1;
static constexpr uint32_t LOBBS_YARN_DEFAULT_MAX_CHARS = 32;

class YarnDal
{
  public:
    explicit YarnDal(LoDb &lodb);

    bool formatYarnView(char *out, size_t outCap);
    uint32_t newWordsForUser(uint64_t userUuid);
    bool markYarnSeen(uint64_t userUuid);
    bool appendWords(uint64_t userUuid, bool isSysop, const char *const *words, int wordCount, char *err, size_t errCap);
    bool setConfig(uint32_t periodSeconds, uint32_t maxWords, uint32_t maxChars, char *err, size_t errCap);
    uint32_t totalWordsAppended();

  private:
    bool loadCurrent(meshtastic_LoBBSYarnCurrent &out);
    bool saveCurrent(meshtastic_LoBBSYarnCurrent &cur);
    bool loadConfig(meshtastic_LoBBSYarnConfig &out);
    bool saveQuota(meshtastic_LoBBSYarnQuota &quota);
    bool checkAppendQuota(uint64_t userUuid, bool isSysop, uint32_t wordCount, uint32_t charCost, char *err, size_t errCap);
    bool recordAppendQuota(uint64_t userUuid, uint32_t wordCount, uint32_t charCost);
    static void trimTailToMax(char *text);
    static bool isValidWordToken(const char *word);
    LoDb &lodb_;
};

#endif
