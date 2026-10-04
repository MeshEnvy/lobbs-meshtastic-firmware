#if !MESHTASTIC_EXCLUDE_LOBBS

#include "YarnDal.h"
#include "../AppUtil.h"
#include "YarnRecords.h"
#include <cstdio>
#include <cstring>

YarnDal::YarnDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("yarn_current");
    lodb_.registerTable("yarn_seen");
    lodb_.registerTable("yarn_config");
    lodb_.registerTable("yarn_quota");
}

static lodb_uuid_t yarnCurrentRecordUuid()
{
    return lodb_new_uuid("lobbs_yarn_current_v1", 0);
}

static lodb_uuid_t yarnConfigRecordUuid()
{
    return lodb_new_uuid("lobbs_yarn_config_v1", 0);
}

static lodb_uuid_t yarnSeenRecordUuid(uint64_t userUuid)
{
    char hex[17];
    lodb_uuid_to_hex(userUuid, hex);
    return lodb_new_uuid(hex, 0);
}

void YarnDal::loadCurrent(CurrentState &out)
{
    out.text[0] = '\0';
    out.total_words_appended = 0;
    LoScalar rec;
    if (lodb_.get("yarn_current", yarnCurrentRecordUuid(), rec) != LODB_OK)
        return;
    std::string text;
    if (rec.getString(LODB_F_DESCRIPTION, text))
        strncpy(out.text, text.c_str(), LOBBS_YARN_BODY_MAX);
    out.text[LOBBS_YARN_BODY_MAX] = '\0';
    rec.getUint32(YarnCurrentField::FIELD_TOTAL_WORDS, out.total_words_appended);
}

bool YarnDal::saveCurrent(CurrentState &cur)
{
    lodb_uuid_t id = yarnCurrentRecordUuid();
    LoScalar rec;
    rec.setString(LODB_F_DESCRIPTION, cur.text);
    rec.setUint32(YarnCurrentField::FIELD_TOTAL_WORDS, cur.total_words_appended);
    return lodb_.upsert("yarn_current", id, rec) == LODB_OK;
}

void YarnDal::loadConfig(ConfigState &out)
{
    out.period_seconds = LOBBS_YARN_DEFAULT_PERIOD_SEC;
    out.max_words_per_interval = LOBBS_YARN_DEFAULT_MAX_WORDS;
    out.max_chars_per_interval = LOBBS_YARN_DEFAULT_MAX_CHARS;
    LoScalar rec;
    if (lodb_.get("yarn_config", yarnConfigRecordUuid(), rec) != LODB_OK)
        return;
    rec.getUint32(YarnConfigField::FIELD_PERIOD_SEC, out.period_seconds);
    rec.getUint32(YarnConfigField::FIELD_MAX_WORDS, out.max_words_per_interval);
    rec.getUint32(YarnConfigField::FIELD_MAX_CHARS, out.max_chars_per_interval);
}

const char *YarnDal::setConfig(uint32_t periodSeconds, uint32_t maxWords, uint32_t maxChars)
{
    if (periodSeconds < 60 || periodSeconds > 86400)
        return "Bad period.";
    if (maxWords < 1 || maxWords > 1000)
        return "Bad word max.";
    if (maxChars < 1 || maxChars > LOBBS_YARN_BODY_MAX)
        return "Bad char max.";
    LoScalar cfg;
    cfg.setUint32(YarnConfigField::FIELD_PERIOD_SEC, periodSeconds);
    cfg.setUint32(YarnConfigField::FIELD_MAX_WORDS, maxWords);
    cfg.setUint32(YarnConfigField::FIELD_MAX_CHARS, maxChars);
    return lodb_.upsert("yarn_config", yarnConfigRecordUuid(), cfg) == LODB_OK ? nullptr : "Failed.";
}

void YarnDal::trimTailToMax(char *text)
{
    if (!text)
        return;
    while (strlen(text) > LOBBS_YARN_BODY_MAX) {
        char *sp = strchr(text, ' ');
        if (!sp) {
            text[LOBBS_YARN_BODY_MAX] = '\0';
            return;
        }
        memmove(text, sp + 1, strlen(sp + 1) + 1);
    }
}

bool YarnDal::isValidWordToken(const char *word)
{
    if (!word || !word[0])
        return false;
    for (const char *p = word; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x21 || c > 0x7e)
            return false;
    }
    return true;
}

bool YarnDal::formatYarnView(char *out, size_t outCap)
{
    if (!out || outCap == 0)
        return false;
    CurrentState cur = {};
    loadCurrent(cur);
    if (!cur.text[0]) {
        snprintf(out, outCap, "Yarn empty.");
        return true;
    }
    snprintf(out, outCap, "%s", cur.text);
    return true;
}

uint32_t YarnDal::totalWordsAppended()
{
    CurrentState cur = {};
    loadCurrent(cur);
    return cur.total_words_appended;
}

uint32_t YarnDal::newWordsForUser(uint64_t userUuid)
{
    CurrentState cur = {};
    loadCurrent(cur);
    LoScalar seen;
    if (lodb_.get("yarn_seen", yarnSeenRecordUuid(userUuid), seen) != LODB_OK)
        return cur.total_words_appended;
    uint32_t lastSeen = 0;
    seen.getUint32(YarnSeenField::FIELD_LAST_TOTAL_WORDS, lastSeen);
    if (cur.total_words_appended <= lastSeen)
        return 0;
    return cur.total_words_appended - lastSeen;
}

bool YarnDal::markYarnSeen(uint64_t userUuid)
{
    CurrentState cur = {};
    loadCurrent(cur);
    LoScalar seen;
    seen.setUint64(YarnSeenField::FIELD_USER_UUID, userUuid);
    seen.setUint32(YarnSeenField::FIELD_LAST_TOTAL_WORDS, cur.total_words_appended);
    lodb_uuid_t id = yarnSeenRecordUuid(userUuid);
    return lodb_.upsert("yarn_seen", id, seen) == LODB_OK;
}

const char *YarnDal::appendWords(uint64_t userUuid, bool isSysop, const char *const *words, int wordCount)
{
    if (!words || wordCount <= 0)
        return "No words.";
    for (int i = 0; i < wordCount; i++) {
        if (!isValidWordToken(words[i]))
            return "Bad word.";
        if (strlen(words[i]) > LOBBS_YARN_BODY_MAX)
            return "Word too long.";
    }
    CurrentState cur = {};
    loadCurrent(cur);

    uint32_t charCost = 0;
    bool needSpace = cur.text[0] != '\0';
    for (int i = 0; i < wordCount; i++) {
        if (needSpace)
            charCost += 1;
        charCost += (uint32_t)strlen(words[i]);
        needSpace = true;
    }

    ConfigState cfg = {};
    loadConfig(cfg);
    // Quota counters: {words, chars}
    uint32_t add[2] = {(uint32_t)wordCount, charCost};
    if (!isSysop) {
        if (add[0] > cfg.max_words_per_interval || add[1] > cfg.max_chars_per_interval)
            return "Too many words.";
        uint32_t used[2];
        lobbsQuotaUsed(lodb_, "yarn_quota", userUuid, cfg.period_seconds, used, 2);
        if (used[0] + add[0] > cfg.max_words_per_interval)
            return "Quota full.";
        if (used[1] + add[1] > cfg.max_chars_per_interval)
            return "Too many chars.";
    }

    char candidate[LOBBS_YARN_BODY_MAX + 64];
    candidate[0] = '\0';
    if (cur.text[0])
        strncpy(candidate, cur.text, sizeof(candidate) - 1);
    candidate[sizeof(candidate) - 1] = '\0';
    size_t pos = strlen(candidate);
    for (int i = 0; i < wordCount; i++) {
        const char *w = words[i];
        size_t wlen = strlen(w);
        if (pos > 0) {
            if (pos + 1 >= sizeof(candidate))
                break;
            candidate[pos++] = ' ';
        }
        if (pos + wlen >= sizeof(candidate))
            break;
        memcpy(candidate + pos, w, wlen);
        pos += wlen;
        candidate[pos] = '\0';
    }
    trimTailToMax(candidate);
    if (strlen(candidate) > LOBBS_YARN_BODY_MAX)
        return "Too many chars.";
    strncpy(cur.text, candidate, LOBBS_YARN_BODY_MAX);
    cur.text[LOBBS_YARN_BODY_MAX] = '\0';
    cur.total_words_appended += (uint32_t)wordCount;
    if (!saveCurrent(cur))
        return "Save failed.";
    if (!isSysop && !lobbsQuotaAdd(lodb_, "yarn_quota", userUuid, cfg.period_seconds, add, 2))
        return "Quota save.";
    return nullptr;
}

#endif
