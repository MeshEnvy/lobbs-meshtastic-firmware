#if !MESHTASTIC_EXCLUDE_LOBBS

#include "WallDal.h"
#include "gps/RTC.h"
#include <cctype>
#include <cstdio>
#include <cstring>

WallDal::WallDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("wall_canvas", &meshtastic_LoBBSWallCanvas_msg, sizeof(meshtastic_LoBBSWallCanvas));
    lodb_.registerTable("wall_seen", &meshtastic_LoBBSWallSeen_msg, sizeof(meshtastic_LoBBSWallSeen));
    lodb_.registerTable("wall_config", &meshtastic_LoBBSWallConfig_msg, sizeof(meshtastic_LoBBSWallConfig));
    lodb_.registerTable("wall_quota", &meshtastic_LoBBSWallQuota_msg, sizeof(meshtastic_LoBBSWallQuota));
}

static lodb_uuid_t wallCanvasRecordUuid()
{
    return lodb_new_uuid("lobbs_wall_canvas_v1", 0);
}

static lodb_uuid_t wallConfigRecordUuid()
{
    return lodb_new_uuid("lobbs_wall_config_v1", 0);
}

static lodb_uuid_t wallSeenRecordUuid(uint64_t userUuid)
{
    char hex[17];
    lodb_uuid_to_hex(userUuid, hex);
    return lodb_new_uuid(hex, 0);
}

static lodb_uuid_t wallQuotaRecordUuid(uint64_t userUuid)
{
    char hex[17];
    lodb_uuid_to_hex(userUuid, hex);
    char key[24];
    snprintf(key, sizeof(key), "q:%s", hex);
    return lodb_new_uuid(key, 0);
}

static void wallSealCanvas(meshtastic_LoBBSWallCanvas &canvas)
{
    canvas.cells[LOBBS_WALL_CELLS] = '\0';
}

uint32_t WallDal::computeCrc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xedb88320 & (~((crc & 1) - 1)));
    }
    return ~crc;
}

bool WallDal::loadCanvas(meshtastic_LoBBSWallCanvas &out)
{
    lodb_uuid_t id = wallCanvasRecordUuid();
    if (lodb_.get("wall_canvas", id, &out) == LODB_OK) {
        wallSealCanvas(out);
        return true;
    }
    out = meshtastic_LoBBSWallCanvas_init_zero;
    for (int i = 0; i < LOBBS_WALL_CELLS; i++)
        out.cells[i] = ' ';
    wallSealCanvas(out);
    out.crc32 = computeCrc32((const uint8_t *)out.cells, LOBBS_WALL_CELLS);
    lodb_.deleteRecord("wall_canvas", id);
    if (lodb_.insert("wall_canvas", id, &out) != LODB_OK)
        return false;
    return true;
}

bool WallDal::saveCanvas(meshtastic_LoBBSWallCanvas &canvas)
{
    wallSealCanvas(canvas);
    canvas.crc32 = computeCrc32((const uint8_t *)canvas.cells, LOBBS_WALL_CELLS);
    lodb_uuid_t id = wallCanvasRecordUuid();
    lodb_.deleteRecord("wall_canvas", id);
    return lodb_.insert("wall_canvas", id, &canvas) == LODB_OK;
}

bool WallDal::loadConfig(meshtastic_LoBBSWallConfig &out)
{
    lodb_uuid_t id = wallConfigRecordUuid();
    if (lodb_.get("wall_config", id, &out) == LODB_OK)
        return true;
    meshtastic_LoBBSWallConfig fresh = meshtastic_LoBBSWallConfig_init_zero;
    fresh.period_seconds = LOBBS_WALL_DEFAULT_PERIOD_SEC;
    fresh.max_cells_per_cycle = LOBBS_WALL_DEFAULT_MAX_CELLS;
    lodb_.deleteRecord("wall_config", id);
    if (lodb_.insert("wall_config", id, &fresh) != LODB_OK)
        return false;
    out = fresh;
    return true;
}

bool WallDal::setConfig(uint32_t periodSeconds, uint32_t maxCellsPerCycle, char *err, size_t errCap)
{
    if (periodSeconds < 60 || periodSeconds > 86400) {
        if (err && errCap)
            snprintf(err, errCap, "Bad period.");
        return false;
    }
    if (maxCellsPerCycle < 1 || maxCellsPerCycle > LOBBS_WALL_CELLS) {
        if (err && errCap)
            snprintf(err, errCap, "Bad cell max.");
        return false;
    }
    meshtastic_LoBBSWallConfig cfg = meshtastic_LoBBSWallConfig_init_zero;
    lodb_uuid_t id = wallConfigRecordUuid();
    cfg.period_seconds = periodSeconds;
    cfg.max_cells_per_cycle = maxCellsPerCycle;
    lodb_.deleteRecord("wall_config", id);
    return lodb_.insert("wall_config", id, &cfg) == LODB_OK;
}

bool WallDal::saveQuota(meshtastic_LoBBSWallQuota &quota)
{
    lodb_uuid_t id = wallQuotaRecordUuid(quota.user_uuid);
    lodb_.deleteRecord("wall_quota", id);
    return lodb_.insert("wall_quota", id, &quota) == LODB_OK;
}

bool WallDal::checkPaintQuota(uint64_t userUuid, bool isSysop, int tokenCount, char *err, size_t errCap)
{
    if (isSysop)
        return true;
    if (tokenCount <= 0) {
        if (err && errCap)
            snprintf(err, errCap, "No paint tokens.");
        return false;
    }
    meshtastic_LoBBSWallConfig cfg = meshtastic_LoBBSWallConfig_init_zero;
    if (!loadConfig(cfg)) {
        if (err && errCap)
            snprintf(err, errCap, "Config error.");
        return false;
    }
    if ((uint32_t)tokenCount > cfg.max_cells_per_cycle) {
        if (err && errCap)
            snprintf(err, errCap, "Too many cells.");
        return false;
    }
    meshtastic_LoBBSWallQuota quota = meshtastic_LoBBSWallQuota_init_zero;
    lodb_uuid_t id = wallQuotaRecordUuid(userUuid);
    if (lodb_.get("wall_quota", id, &quota) != LODB_OK) {
        quota.user_uuid = userUuid;
        quota.cycle_start = 0;
        quota.cells_used = 0;
    }
    uint32_t now = getTime();
    uint32_t used = quota.cells_used;
    if (quota.cycle_start == 0 || now >= quota.cycle_start + cfg.period_seconds)
        used = 0;
    if (used + (uint32_t)tokenCount > cfg.max_cells_per_cycle) {
        if (err && errCap)
            snprintf(err, errCap, "Quota full.");
        return false;
    }
    return true;
}

bool WallDal::recordPaintQuota(uint64_t userUuid, int tokenCount)
{
    meshtastic_LoBBSWallConfig cfg = meshtastic_LoBBSWallConfig_init_zero;
    if (!loadConfig(cfg))
        return false;
    meshtastic_LoBBSWallQuota quota = meshtastic_LoBBSWallQuota_init_zero;
    lodb_uuid_t id = wallQuotaRecordUuid(userUuid);
    if (lodb_.get("wall_quota", id, &quota) != LODB_OK) {
        quota.user_uuid = userUuid;
        quota.cycle_start = 0;
        quota.cells_used = 0;
    }
    uint32_t now = getTime();
    if (quota.cycle_start == 0 || now >= quota.cycle_start + cfg.period_seconds) {
        quota.cycle_start = now;
        quota.cells_used = 0;
    }
    quota.cells_used += (uint32_t)tokenCount;
    quota.user_uuid = userUuid;
    return saveQuota(quota);
}

bool WallDal::formatGridLines(char *out, size_t outCap)
{
    meshtastic_LoBBSWallCanvas canvas = meshtastic_LoBBSWallCanvas_init_zero;
    if (!loadCanvas(canvas))
        return false;
    size_t n = 0;
    for (int row = 0; row < LOBBS_WALL_ROWS; row++) {
        if (row > 0 && n + 1 < outCap)
            out[n++] = '\n';
        for (int col = 0; col < LOBBS_WALL_COLS; col++) {
            if (n + 1 >= outCap)
                return false;
            out[n++] = canvas.cells[row * LOBBS_WALL_COLS + col];
        }
    }
    if (n >= outCap)
        return false;
    out[n] = '\0';
    return true;
}

uint32_t WallDal::canvasCrc32()
{
    meshtastic_LoBBSWallCanvas canvas = meshtastic_LoBBSWallCanvas_init_zero;
    if (!loadCanvas(canvas))
        return 0;
    return canvas.crc32;
}

uint32_t WallDal::getLastSeenCrc(uint64_t userUuid)
{
    meshtastic_LoBBSWallSeen seen = meshtastic_LoBBSWallSeen_init_zero;
    if (lodb_.get("wall_seen", wallSeenRecordUuid(userUuid), &seen) != LODB_OK)
        return 0xffffffff;
    return seen.last_crc32;
}

bool WallDal::markSeen(uint64_t userUuid, uint32_t crc32)
{
    meshtastic_LoBBSWallSeen seen = meshtastic_LoBBSWallSeen_init_zero;
    seen.user_uuid = userUuid;
    seen.last_crc32 = crc32;
    lodb_uuid_t id = wallSeenRecordUuid(userUuid);
    lodb_.deleteRecord("wall_seen", id);
    return lodb_.insert("wall_seen", id, &seen) == LODB_OK;
}

bool WallDal::isDirtyForUser(uint64_t userUuid)
{
    uint32_t cur = canvasCrc32();
    uint32_t last = getLastSeenCrc(userUuid);
    if (last == 0xffffffff)
        return true;
    return cur != last;
}

static bool parseCoord(const char *tok, int &rowOut, int &colOut, const char **endOut)
{
    if (!tok || !tok[0])
        return false;
    char r = (char)tolower((unsigned char)tok[0]);
    if (r < 'a' || r > 'l')
        return false;
    rowOut = r - 'a';
    const char *p = tok + 1;
    if (!isdigit((unsigned char)*p))
        return false;
    int col = 0;
    while (isdigit((unsigned char)*p)) {
        col = col * 10 + (*p - '0');
        p++;
    }
    if (col < 1 || col > LOBBS_WALL_COLS)
        return false;
    colOut = col;
    if (endOut)
        *endOut = p;
    return true;
}

static bool parseWallToken(const char *tok, int &rowOut, int &colOut, char &chOut)
{
    if (!tok || !tok[0])
        return false;
    const char *coord = tok;
    if (tok[0] == '-') {
        coord = tok + 1;
        if (!coord[0])
            return false;
        const char *end = nullptr;
        if (!parseCoord(coord, rowOut, colOut, &end) || !end || *end != '\0')
            return false;
        chOut = ' ';
        return true;
    }
    const char *end = nullptr;
    if (!parseCoord(coord, rowOut, colOut, &end) || !end || end[1] != '\0')
        return false;
    chOut = *end;
    if ((unsigned char)chOut < 0x20 || (unsigned char)chOut > 0x7e)
        return false;
    return true;
}

bool WallDal::applyPaintTokens(uint64_t userUuid, bool isSysop, const char *const *tokens, int count, char *err,
                                 size_t errCap)
{
    if (count <= 0) {
        if (err && errCap)
            snprintf(err, errCap, "No paint tokens.");
        return false;
    }
    meshtastic_LoBBSWallCanvas canvas = meshtastic_LoBBSWallCanvas_init_zero;
    if (!loadCanvas(canvas)) {
        if (err && errCap)
            snprintf(err, errCap, "Canvas error.");
        return false;
    }
    for (int i = 0; i < count; i++) {
        int row = 0;
        int col = 0;
        char ch = 0;
        if (!parseWallToken(tokens[i], row, col, ch)) {
            if (err && errCap)
                snprintf(err, errCap, "Bad token.");
            return false;
        }
        canvas.cells[row * LOBBS_WALL_COLS + (col - 1)] = ch;
    }
    wallSealCanvas(canvas);
    if (!checkPaintQuota(userUuid, isSysop, count, err, errCap))
        return false;
    if (!saveCanvas(canvas)) {
        if (err && errCap)
            snprintf(err, errCap, "Save failed.");
        return false;
    }
    if (!isSysop && !recordPaintQuota(userUuid, count)) {
        if (err && errCap)
            snprintf(err, errCap, "Quota save.");
        return false;
    }
    return true;
}

#endif
