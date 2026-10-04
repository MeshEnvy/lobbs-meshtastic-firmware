#if !MESHTASTIC_EXCLUDE_LOBBS

#include "WallDal.h"
#include "WallRecords.h"
#include "gps/RTC.h"
#include <cctype>
#include <cstdio>
#include <cstring>

WallDal::WallDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("wall_canvas");
    lodb_.registerTable("wall_seen");
    lodb_.registerTable("wall_config");
    lodb_.registerTable("wall_quota");
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

static void wallSealCells(char *cells)
{
    cells[LOBBS_WALL_CELLS] = '\0';
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

bool WallDal::loadCanvas(CanvasState &out)
{
    lodb_uuid_t id = wallCanvasRecordUuid();
    LoScalar rec;
    if (lodb_.get("wall_canvas", id, rec) == LODB_OK) {
        std::string cells;
        if (rec.getString(WallCanvasField::FIELD_CELLS, cells)) {
            strncpy(out.cells, cells.c_str(), LOBBS_WALL_CELLS);
            out.cells[LOBBS_WALL_CELLS] = '\0';
        } else {
            memset(out.cells, ' ', LOBBS_WALL_CELLS);
            out.cells[LOBBS_WALL_CELLS] = '\0';
        }
        rec.getUint32(WallCanvasField::FIELD_CRC32, out.crc32);
        wallSealCells(out.cells);
        return true;
    }
    memset(out.cells, ' ', LOBBS_WALL_CELLS);
    out.cells[LOBBS_WALL_CELLS] = '\0';
    wallSealCells(out.cells);
    out.crc32 = computeCrc32((const uint8_t *)out.cells, LOBBS_WALL_CELLS);
    return true;
}

bool WallDal::saveCanvas(CanvasState &canvas)
{
    wallSealCells(canvas.cells);
    canvas.crc32 = computeCrc32((const uint8_t *)canvas.cells, LOBBS_WALL_CELLS);
    lodb_uuid_t id = wallCanvasRecordUuid();
    LoScalar rec;
    rec.setString(WallCanvasField::FIELD_CELLS, canvas.cells);
    rec.setUint32(WallCanvasField::FIELD_CRC32, canvas.crc32);
    return lodb_.upsert("wall_canvas", id, rec) == LODB_OK;
}

bool WallDal::loadConfig(ConfigState &out)
{
    lodb_uuid_t id = wallConfigRecordUuid();
    LoScalar rec;
    if (lodb_.get("wall_config", id, rec) == LODB_OK) {
        rec.getUint32(WallConfigField::FIELD_PERIOD_SEC, out.period_seconds);
        rec.getUint32(WallConfigField::FIELD_MAX_CELLS, out.max_cells_per_cycle);
        return true;
    }
    out.period_seconds = LOBBS_WALL_DEFAULT_PERIOD_SEC;
    out.max_cells_per_cycle = LOBBS_WALL_DEFAULT_MAX_CELLS;
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
    lodb_uuid_t id = wallConfigRecordUuid();
    LoScalar cfg;
    cfg.setUint32(WallConfigField::FIELD_PERIOD_SEC, periodSeconds);
    cfg.setUint32(WallConfigField::FIELD_MAX_CELLS, maxCellsPerCycle);
    return lodb_.upsert("wall_config", id, cfg) == LODB_OK;
}

bool WallDal::saveQuota(QuotaState &quota)
{
    lodb_uuid_t id = wallQuotaRecordUuid(quota.user_uuid);
    LoScalar rec;
    rec.setUint64(WallQuotaField::FIELD_USER_UUID, quota.user_uuid);
    rec.setUint32(WallQuotaField::FIELD_CYCLE_START, quota.cycle_start);
    rec.setUint32(WallQuotaField::FIELD_CELLS_USED, quota.cells_used);
    return lodb_.upsert("wall_quota", id, rec) == LODB_OK;
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
    ConfigState cfg = {};
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
    QuotaState quota = {};
    lodb_uuid_t id = wallQuotaRecordUuid(userUuid);
    LoScalar qrec;
    if (lodb_.get("wall_quota", id, qrec) == LODB_OK) {
        qrec.getUint64(WallQuotaField::FIELD_USER_UUID, quota.user_uuid);
        qrec.getUint32(WallQuotaField::FIELD_CYCLE_START, quota.cycle_start);
        qrec.getUint32(WallQuotaField::FIELD_CELLS_USED, quota.cells_used);
    } else {
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
    ConfigState cfg = {};
    if (!loadConfig(cfg))
        return false;
    QuotaState quota = {};
    lodb_uuid_t id = wallQuotaRecordUuid(userUuid);
    LoScalar qrec;
    if (lodb_.get("wall_quota", id, qrec) == LODB_OK) {
        qrec.getUint64(WallQuotaField::FIELD_USER_UUID, quota.user_uuid);
        qrec.getUint32(WallQuotaField::FIELD_CYCLE_START, quota.cycle_start);
        qrec.getUint32(WallQuotaField::FIELD_CELLS_USED, quota.cells_used);
    } else {
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
    CanvasState canvas = {};
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
    CanvasState canvas = {};
    if (!loadCanvas(canvas))
        return 0;
    return canvas.crc32;
}

uint32_t WallDal::getLastSeenCrc(uint64_t userUuid)
{
    LoScalar seen;
    if (lodb_.get("wall_seen", wallSeenRecordUuid(userUuid), seen) != LODB_OK)
        return 0xffffffff;
    uint32_t crc = 0;
    seen.getUint32(WallSeenField::FIELD_CRC32, crc);
    return crc;
}

bool WallDal::markSeen(uint64_t userUuid, uint32_t crc32Val)
{
    LoScalar seen;
    seen.setUint64(WallSeenField::FIELD_USER_UUID, userUuid);
    seen.setUint32(WallSeenField::FIELD_CRC32, crc32Val);
    lodb_uuid_t id = wallSeenRecordUuid(userUuid);
    return lodb_.upsert("wall_seen", id, seen) == LODB_OK;
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

bool WallDal::applyPaintTokens(uint64_t userUuid, bool isSysop, const char *const *tokens, int count, char *err, size_t errCap)
{
    if (count <= 0) {
        if (err && errCap)
            snprintf(err, errCap, "No paint tokens.");
        return false;
    }
    CanvasState canvas = {};
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
    wallSealCells(canvas.cells);
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
