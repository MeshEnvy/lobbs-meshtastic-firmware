#if !MESHTASTIC_EXCLUDE_LOBBS

#include "YarnDal.h"
#include "YarnRecords.h"
#include "gps/RTC.h"
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

static lodb_uuid_t yarnQuotaRecordUuid(uint64_t userUuid)
{
    char hex[17];
    lodb_uuid_to_hex(userUuid, hex);
    char key[24];
    snprintf(key, sizeof(key), "yq:%s", hex);
    return lodb_new_uuid(key, 0);
}

bool YarnDal::loadCurrent(CurrentState &out)
{
    lodb_uuid_t id = yarnCurrentRecordUuid();
    LoScalar rec;
    if (lodb_.get("yarn_current", id, rec) == LODB_OK) {
        std::string text;
        if (rec.getString(LODB_F_DESCRIPTION, text))
            strncpy(out.text, text.c_str(), LOBBS_YARN_BODY_MAX);
        else
            out.text[0] = '\0';
        out.text[LOBBS_YARN_BODY_MAX] = '\0';
        rec.getUint32(YarnCurrentField::FIELD_TOTAL_WORDS, out.total_words_appended);
        return true;
    }
    out.text[0] = '\0';
    out.total_words_appended = 0;
    lodb_.deleteRecord("yarn_current", id);
    LoScalar fresh;
    fresh.setString(LODB_F_DESCRIPTION, "");
    fresh.setUint32(YarnCurrentField::FIELD_TOTAL_WORDS, 0);
    if (lodb_.insert("yarn_current", id, fresh) != LODB_OK)
        return false;
    return true;
}

bool YarnDal::saveCurrent(CurrentState &cur)
{
    lodb_uuid_t id = yarnCurrentRecordUuid();
    LoScalar rec;
    rec.setString(LODB_F_DESCRIPTION, cur.text);
    rec.setUint32(YarnCurrentField::FIELD_TOTAL_WORDS, cur.total_words_appended);
    rec.removeField(LODB_F_CREATED);
    rec.removeField(LODB_F_UPDATED);
    LoScalar existing;
    if (lodb_.get("yarn_current", id, existing) == LODB_OK)
        return lodb_.update("yarn_current", id, rec) == LODB_OK;
    return lodb_.insert("yarn_current", id, rec) == LODB_OK;
}

bool YarnDal::loadConfig(ConfigState &out)
{
    lodb_uuid_t id = yarnConfigRecordUuid();
    LoScalar rec;
    if (lodb_.get("yarn_config", id, rec) == LODB_OK) {
        rec.getUint32(YarnConfigField::FIELD_PERIOD_SEC, out.period_seconds);
        rec.getUint32(YarnConfigField::FIELD_MAX_WORDS, out.max_words_per_interval);
        rec.getUint32(YarnConfigField::FIELD_MAX_CHARS, out.max_chars_per_interval);
        return true;
    }
    out.period_seconds = LOBBS_YARN_DEFAULT_PERIOD_SEC;
    out.max_words_per_interval = LOBBS_YARN_DEFAULT_MAX_WORDS;
    out.max_chars_per_interval = LOBBS_YARN_DEFAULT_MAX_CHARS;
    lodb_.deleteRecord("yarn_config", id);
    LoScalar fresh;
    fresh.setUint32(YarnConfigField::FIELD_PERIOD_SEC, out.period_seconds);
    fresh.setUint32(YarnConfigField::FIELD_MAX_WORDS, out.max_words_per_interval);
    fresh.setUint32(YarnConfigField::FIELD_MAX_CHARS, out.max_chars_per_interval);
    if (lodb_.insert("yarn_config", id, fresh) != LODB_OK)
        return false;
    return true;
}

bool YarnDal::setConfig(uint32_t periodSeconds, uint32_t maxWords, uint32_t maxChars, char *err, size_t errCap)
{
    if (periodSeconds < 60 || periodSeconds > 86400) {
        if (err && errCap)
            snprintf(err, errCap, "Bad period.");
        return false;
    }
    if (maxWords < 1 || maxWords > 1000) {
        if (err && errCap)
            snprintf(err, errCap, "Bad word max.");
        return false;
    }
    if (maxChars < 1 || maxChars > LOBBS_YARN_BODY_MAX) {
        if (err && errCap)
            snprintf(err, errCap, "Bad char max.");
        return false;
    }
    lodb_uuid_t id = yarnConfigRecordUuid();
    LoScalar cfg;
    cfg.setUint32(YarnConfigField::FIELD_PERIOD_SEC, periodSeconds);
    cfg.setUint32(YarnConfigField::FIELD_MAX_WORDS, maxWords);
    cfg.setUint32(YarnConfigField::FIELD_MAX_CHARS, maxChars);
    cfg.removeField(LODB_F_CREATED);
    cfg.removeField(LODB_F_UPDATED);
    LoScalar existing;
    if (lodb_.get("yarn_config", id, existing) == LODB_OK)
        return lodb_.update("yarn_config", id, cfg) == LODB_OK;
    return lodb_.insert("yarn_config", id, cfg) == LODB_OK;
}

bool YarnDal::saveQuota(QuotaState &quota)
{
    lodb_uuid_t id = yarnQuotaRecordUuid(quota.user_uuid);
    LoScalar rec;
    rec.setUint64(YarnQuotaField::FIELD_USER_UUID, quota.user_uuid);
    rec.setUint32(YarnQuotaField::FIELD_CYCLE_START, quota.cycle_start);
    rec.setUint32(YarnQuotaField::FIELD_WORDS_USED, quota.words_used);
    rec.setUint32(YarnQuotaField::FIELD_CHARS_USED, quota.chars_used);
    rec.removeField(LODB_F_CREATED);
    rec.removeField(LODB_F_UPDATED);
    LoScalar existing;
    if (lodb_.get("yarn_quota", id, existing) == LODB_OK)
        return lodb_.update("yarn_quota", id, rec) == LODB_OK;
    return lodb_.insert("yarn_quota", id, rec) == LODB_OK;
}

bool YarnDal::checkAppendQuota(uint64_t userUuid, bool isSysop, uint32_t wordCount, uint32_t charCost, char *err, size_t errCap)
{
    if (isSysop)
        return true;
    if (wordCount == 0)
        return true;
    ConfigState cfg = {};
    if (!loadConfig(cfg)) {
        if (err && errCap)
            snprintf(err, errCap, "Config error.");
        return false;
    }
    if (wordCount > cfg.max_words_per_interval || charCost > cfg.max_chars_per_interval) {
        if (err && errCap)
            snprintf(err, errCap, "Too many words.");
        return false;
    }
    QuotaState quota = {};
    lodb_uuid_t id = yarnQuotaRecordUuid(userUuid);
    LoScalar qrec;
    if (lodb_.get("yarn_quota", id, qrec) == LODB_OK) {
        qrec.getUint64(YarnQuotaField::FIELD_USER_UUID, quota.user_uuid);
        qrec.getUint32(YarnQuotaField::FIELD_CYCLE_START, quota.cycle_start);
        qrec.getUint32(YarnQuotaField::FIELD_WORDS_USED, quota.words_used);
        qrec.getUint32(YarnQuotaField::FIELD_CHARS_USED, quota.chars_used);
    } else {
        quota.user_uuid = userUuid;
        quota.cycle_start = 0;
        quota.words_used = 0;
        quota.chars_used = 0;
    }
    uint32_t now = getTime();
    uint32_t words = quota.words_used;
    uint32_t chars = quota.chars_used;
    if (quota.cycle_start == 0 || now >= quota.cycle_start + cfg.period_seconds) {
        words = 0;
        chars = 0;
    }
    if (words + wordCount > cfg.max_words_per_interval) {
        if (err && errCap)
            snprintf(err, errCap, "Quota full.");
        return false;
    }
    if (chars + charCost > cfg.max_chars_per_interval) {
        if (err && errCap)
            snprintf(err, errCap, "Too many chars.");
        return false;
    }
    return true;
}

bool YarnDal::recordAppendQuota(uint64_t userUuid, uint32_t wordCount, uint32_t charCost)
{
    if (wordCount == 0)
        return true;
    ConfigState cfg = {};
    if (!loadConfig(cfg))
        return false;
    QuotaState quota = {};
    lodb_uuid_t id = yarnQuotaRecordUuid(userUuid);
    LoScalar qrec;
    if (lodb_.get("yarn_quota", id, qrec) == LODB_OK) {
        qrec.getUint64(YarnQuotaField::FIELD_USER_UUID, quota.user_uuid);
        qrec.getUint32(YarnQuotaField::FIELD_CYCLE_START, quota.cycle_start);
        qrec.getUint32(YarnQuotaField::FIELD_WORDS_USED, quota.words_used);
        qrec.getUint32(YarnQuotaField::FIELD_CHARS_USED, quota.chars_used);
    } else {
        quota.user_uuid = userUuid;
        quota.cycle_start = 0;
        quota.words_used = 0;
        quota.chars_used = 0;
    }
    uint32_t now = getTime();
    if (quota.cycle_start == 0 || now >= quota.cycle_start + cfg.period_seconds) {
        quota.cycle_start = now;
        quota.words_used = 0;
        quota.chars_used = 0;
    }
    quota.words_used += wordCount;
    quota.chars_used += charCost;
    quota.user_uuid = userUuid;
    return saveQuota(quota);
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
    if (!loadCurrent(cur))
        return false;
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
    if (!loadCurrent(cur))
        return 0;
    return cur.total_words_appended;
}

uint32_t YarnDal::newWordsForUser(uint64_t userUuid)
{
    CurrentState cur = {};
    if (!loadCurrent(cur))
        return 0;
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
    if (!loadCurrent(cur))
        return false;
    LoScalar seen;
    seen.setUint64(YarnSeenField::FIELD_USER_UUID, userUuid);
    seen.setUint32(YarnSeenField::FIELD_LAST_TOTAL_WORDS, cur.total_words_appended);
    seen.removeField(LODB_F_CREATED);
    seen.removeField(LODB_F_UPDATED);
    lodb_uuid_t id = yarnSeenRecordUuid(userUuid);
    LoScalar existing;
    if (lodb_.get("yarn_seen", id, existing) == LODB_OK)
        return lodb_.update("yarn_seen", id, seen) == LODB_OK;
    return lodb_.insert("yarn_seen", id, seen) == LODB_OK;
}

bool YarnDal::appendWords(uint64_t userUuid, bool isSysop, const char *const *words, int wordCount, char *err, size_t errCap)
{
    if (!words || wordCount <= 0) {
        if (err && errCap)
            snprintf(err, errCap, "No words.");
        return false;
    }
    for (int i = 0; i < wordCount; i++) {
        if (!isValidWordToken(words[i])) {
            if (err && errCap)
                snprintf(err, errCap, "Bad word.");
            return false;
        }
        if (strlen(words[i]) > LOBBS_YARN_BODY_MAX) {
            if (err && errCap)
                snprintf(err, errCap, "Word too long.");
            return false;
        }
    }
    CurrentState cur = {};
    if (!loadCurrent(cur)) {
        if (err && errCap)
            snprintf(err, errCap, "Yarn error.");
        return false;
    }

    uint32_t charCost = 0;
    bool needSpace = cur.text[0] != '\0';
    for (int i = 0; i < wordCount; i++) {
        if (needSpace)
            charCost += 1;
        charCost += (uint32_t)strlen(words[i]);
        needSpace = true;
    }
    if (!checkAppendQuota(userUuid, isSysop, (uint32_t)wordCount, charCost, err, errCap))
        return false;

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
    if (strlen(candidate) > LOBBS_YARN_BODY_MAX) {
        if (err && errCap)
            snprintf(err, errCap, "Too many chars.");
        return false;
    }
    strncpy(cur.text, candidate, LOBBS_YARN_BODY_MAX);
    cur.text[LOBBS_YARN_BODY_MAX] = '\0';
    cur.total_words_appended += (uint32_t)wordCount;
    if (!saveCurrent(cur)) {
        if (err && errCap)
            snprintf(err, errCap, "Save failed.");
        return false;
    }
    if (!isSysop && !recordAppendQuota(userUuid, (uint32_t)wordCount, charCost)) {
        if (err && errCap)
            snprintf(err, errCap, "Quota save.");
        return false;
    }
    return true;
}

#endif
