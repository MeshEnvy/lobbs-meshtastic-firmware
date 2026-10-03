#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

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
    struct CurrentState {
        char text[LOBBS_YARN_BODY_MAX + 1];
        uint32_t total_words_appended;
    };
    struct ConfigState {
        uint32_t period_seconds;
        uint32_t max_words_per_interval;
        uint32_t max_chars_per_interval;
    };
    struct QuotaState {
        uint64_t user_uuid;
        uint32_t cycle_start;
        uint32_t words_used;
        uint32_t chars_used;
    };

    bool loadCurrent(CurrentState &out);
    bool saveCurrent(CurrentState &cur);
    bool loadConfig(ConfigState &out);
    bool saveQuota(QuotaState &quota);
    bool checkAppendQuota(uint64_t userUuid, bool isSysop, uint32_t wordCount, uint32_t charCost, char *err, size_t errCap);
    bool recordAppendQuota(uint64_t userUuid, uint32_t wordCount, uint32_t charCost);
    static void trimTailToMax(char *text);
    static bool isValidWordToken(const char *word);
    LoDb &lodb_;
};

#endif
