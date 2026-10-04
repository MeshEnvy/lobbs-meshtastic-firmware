#include <lodb/LoDB.h>
#include <Arduino.h>
#if __has_include("configuration.h")
#include "configuration.h"
#include "gps/RTC.h"
#undef LODB_LOG_DEBUG
#define LODB_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#undef LODB_LOG_INFO
#define LODB_LOG_INFO(...) LOG_INFO(__VA_ARGS__)
#undef LODB_LOG_WARN
#define LODB_LOG_WARN(...) LOG_WARN(__VA_ARGS__)
#undef LODB_LOG_ERROR
#define LODB_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)
#endif
#include <SHA256.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

static_assert(LODB_FILE_IO_BUFFER_SIZE <= LODB_MAX_RECORD_FILE_BYTES,
              "encode buffer cannot exceed max on-disk record (get would reject writes)");

__attribute__((weak)) uint32_t lodb_now_ms(void)
{
    return static_cast<uint32_t>(millis());
}

__attribute__((weak)) uint32_t lodb_now_unix(void)
{
#if __has_include("configuration.h")
    return getTime();
#else
    return 0;
#endif
}

static void lodbStampInsert(lodb_uuid_t uuid, LoScalar &record)
{
    const uint32_t now = lodb_now_unix();
    record.setUint64(LODB_F_ID, uuid);
    if (!record.has(LODB_F_CREATED))
        record.setUint32(LODB_F_CREATED, now);
    if (!record.has(LODB_F_UPDATED))
        record.setUint32(LODB_F_UPDATED, now);
}

static void lodbStampUpdate(lodb_uuid_t uuid, LoScalar &record, const LoScalar *stored)
{
    const uint32_t now = lodb_now_unix();
    record.setUint64(LODB_F_ID, uuid);
    if (record.has(LODB_F_CREATED)) {
        // caller value kept
    } else if (stored && stored->has(LODB_F_CREATED)) {
        uint32_t created = 0;
        if (stored->getUint32(LODB_F_CREATED, created))
            record.setUint32(LODB_F_CREATED, created);
        else
            record.setUint32(LODB_F_CREATED, now);
    } else {
        record.setUint32(LODB_F_CREATED, now);
    }
    if (!record.has(LODB_F_UPDATED))
        record.setUint32(LODB_F_UPDATED, now);
}

void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17])
{
    snprintf(hex_out, 17, LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
}

lodb_uuid_t lodb_new_uuid(const char *str, uint64_t salt)
{
    char generated_str[32];
    const char *input_str = str;

    if (str == nullptr) {
        uint32_t timestamp = lodb_now_ms();
        uint32_t random_val = (uint32_t)random(0x7fffffff) ^ ((uint32_t)random(0x7fffffff) << 1);
        snprintf(generated_str, sizeof(generated_str), "%u:%u", timestamp, random_val);
        input_str = generated_str;
    }

    SHA256 sha256;
    uint8_t hash[32];

    sha256.reset();
    sha256.update(input_str, strlen(input_str));

    uint8_t salt_bytes[8];
    memcpy(salt_bytes, &salt, 8);
    sha256.update(salt_bytes, 8);

    sha256.finalize(hash, 32);

    lodb_uuid_t uuid;
    memcpy(&uuid, hash, sizeof(lodb_uuid_t));
    return uuid;
}

LoDb::LoDb(const char *db_name, LoFS::FSType filesystem) : db_name(db_name)
{
    static const char kSeg[] = "/lodb";

    if (filesystem == LoFS::FSType::SD) {
        if (LoFS::isSDCardAvailable()) {
            snprintf(db_path, sizeof(db_path), "/sd%s/%s", kSeg, db_name);
        } else {
            LODB_LOG_WARN("SD requested but not available; using /internal%s", kSeg);
            snprintf(db_path, sizeof(db_path), "/internal%s/%s", kSeg, db_name);
        }
    } else if (filesystem == LoFS::FSType::INTERNAL) {
        snprintf(db_path, sizeof(db_path), "/internal%s/%s", kSeg, db_name);
    } else {
        snprintf(db_path, sizeof(db_path), "%s/%s", kSeg, db_name);
    }

    if (strncmp(db_path, "/lodb/", 6) == 0) {
        LoFS::mkdir("/lodb");
    } else if (strncmp(db_path, "/internal/lodb/", 15) == 0) {
        LoFS::mkdir("/internal");
        LoFS::mkdir("/internal/lodb");
    } else if (strncmp(db_path, "/sd/lodb/", 9) == 0) {
        LoFS::mkdir("/sd");
        LoFS::mkdir("/sd/lodb");
    }

    if (!LoFS::mkdir(db_path)) {
        LODB_LOG_DEBUG("Database directory may already exist: %s", db_path);
    }

    LODB_LOG_INFO("Initialized LoDB database: %s", db_path);
}

LoDb::~LoDb() {}

bool LoDb::isLsRecordFile(const std::string &filename)
{
    if (filename.size() < 4)
        return false;
    return filename.compare(filename.size() - 3, 3, ".ls") == 0;
}

LoDbError LoDb::registerTable(const char *table_name)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata metadata;
    metadata.table_name = table_name;

    snprintf(metadata.table_path, sizeof(metadata.table_path), "%s/%s", db_path, table_name);

    if (!LoFS::mkdir(metadata.table_path)) {
        LODB_LOG_DEBUG("Table directory may already exist: %s", metadata.table_path);
    }

    tables[table_name] = metadata;
    LODB_LOG_INFO("Registered table: %s at %s", table_name, metadata.table_path);
    return LODB_OK;
}

LoDb::TableMetadata *LoDb::getTable(const char *table_name)
{
    auto it = tables.find(table_name);
    if (it == tables.end()) {
        LODB_LOG_ERROR("Table not registered: %s", table_name);
        return nullptr;
    }
    return &it->second;
}

static LoDbError writeRecordFile(const char *file_path, const LoScalar &record)
{
    std::string line;
    if (!record.encode(line, LODB_FILE_IO_BUFFER_SIZE)) {
        LODB_LOG_ERROR("Failed to encode LoScalar record");
        return LODB_ERR_ENCODE;
    }

    auto file = LoFS::open(file_path, FILE_O_WRITE);
    if (!file) {
        LODB_LOG_ERROR("Failed to open file for writing: %s", file_path);
        LoFS::remove(file_path);
        return LODB_ERR_IO;
    }

    size_t encoded_size = line.size();
    size_t written = file.write((const uint8_t *)line.data(), encoded_size);
    file.flush();
    file.close();
    if (written != encoded_size) {
        LODB_LOG_ERROR("Failed to write file, wrote %u of %u bytes", (unsigned)written, (unsigned)encoded_size);
        LoFS::remove(file_path);
        return LODB_ERR_IO;
    }

    LODB_LOG_DEBUG("Wrote record to: %s (%u bytes)", file_path, (unsigned)encoded_size);
    return LODB_OK;
}

LoDbError LoDb::insert(const char *table_name, lodb_uuid_t uuid, const LoScalar &record)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.ls", table->table_path, uuid_hex);

    {
        auto existing = LoFS::open(file_path, FILE_O_READ);
        if (existing) {
            size_t existingSize = existing.size();
            existing.close();
            if (existingSize == 0) {
                LoFS::remove(file_path);
            } else {
                LODB_LOG_ERROR("UUID already exists: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
                return LODB_ERR_INVALID;
            }
        }
    }

    LoScalar stamped = record;
    lodbStampInsert(uuid, stamped);
    LoDbError err = writeRecordFile(file_path, stamped);
    if (err != LODB_OK)
        return err;

    LODB_LOG_INFO("Inserted record with custom UUID: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

LoDbError LoDb::get(const char *table_name, lodb_uuid_t uuid, LoScalar &record_out)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.ls", table->table_path, uuid_hex);
    LODB_LOG_DEBUG("file_path: %s", file_path);

    auto file = LoFS::open(file_path, FILE_O_READ);
    if (!file) {
        LODB_LOG_DEBUG("Record not found: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_NOT_FOUND;
    }

    size_t total_size = file.size();
    if (total_size == 0) {
        file.close();
        LoFS::remove(file_path);
        LODB_LOG_WARN("Removed empty record file: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_NOT_FOUND;
    }
    if (total_size > LODB_MAX_RECORD_FILE_BYTES) {
        LODB_LOG_ERROR("Record file too large: %s (%u > %u)", file_path, (unsigned)total_size,
                       (unsigned)LODB_MAX_RECORD_FILE_BYTES);
        file.close();
        LoFS::remove(file_path);
        return LODB_ERR_IO;
    }

    std::unique_ptr<char[]> buffer(new (std::nothrow) char[total_size + 1]);
    if (!buffer) {
        LODB_LOG_ERROR("LoDB get: buffer alloc failed (%u bytes)", (unsigned)total_size);
        file.close();
        return LODB_ERR_IO;
    }

    size_t file_size = file.read((uint8_t *)buffer.get(), total_size);
    file.close();

    if (file_size == 0 || file_size != total_size) {
        LODB_LOG_ERROR("Record read bad size: %s (%u of %u)", file_path, (unsigned)file_size, (unsigned)total_size);
        LoFS::remove(file_path);
        return LODB_ERR_NOT_FOUND;
    }

    buffer[file_size] = '\0';
    while (file_size > 0 && (buffer[file_size - 1] == '\n' || buffer[file_size - 1] == '\r'))
        file_size--;

    record_out.clear();
    if (!record_out.decode(buffer.get(), file_size)) {
        LODB_LOG_ERROR("Failed to decode LoScalar from " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        LoFS::remove(file_path);
        return LODB_ERR_DECODE;
    }

    LODB_LOG_DEBUG("Retrieved record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

LoDbError LoDb::update(const char *table_name, lodb_uuid_t uuid, const LoScalar &record)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.ls", table->table_path, uuid_hex);

    LoScalar stored;
    LoDbError getErr = get(table_name, uuid, stored);
    if (getErr != LODB_OK)
        return getErr;

    LoScalar stamped = record;
    lodbStampUpdate(uuid, stamped, &stored);

    std::string line;
    if (!stamped.encode(line, LODB_FILE_IO_BUFFER_SIZE)) {
        LODB_LOG_ERROR("Failed to encode updated record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_ENCODE;
    }

    char tmp_path[196];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.w", file_path) >= (int)sizeof(tmp_path)) {
        LODB_LOG_ERROR("Temp path too long for update");
        return LODB_ERR_IO;
    }
    LoFS::remove(tmp_path);

    auto file = LoFS::open(tmp_path, FILE_O_WRITE);
    if (!file) {
        LODB_LOG_ERROR("Failed to open temp for update: %s", tmp_path);
        return LODB_ERR_IO;
    }

    size_t encoded_size = line.size();
    size_t written = file.write((const uint8_t *)line.data(), encoded_size);
    file.flush();
    file.close();
    if (written != encoded_size) {
        LODB_LOG_ERROR("Failed to write updated temp file");
        LoFS::remove(tmp_path);
        return LODB_ERR_IO;
    }

    if (!LoFS::rename(tmp_path, file_path)) {
        LODB_LOG_ERROR("Failed to rename temp to: %s", file_path);
        LoFS::remove(tmp_path);
        return LODB_ERR_IO;
    }

    LODB_LOG_INFO("Updated record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

LoDbError LoDb::deleteRecord(const char *table_name, lodb_uuid_t uuid)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        return LODB_ERR_INVALID;
    }

    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);

    char file_path[192];
    snprintf(file_path, sizeof(file_path), "%s/%s.ls", table->table_path, uuid_hex);

    if (LoFS::remove(file_path)) {
        LODB_LOG_DEBUG("Deleted record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_OK;
    }
    LODB_LOG_WARN("Failed to delete record (may not exist): " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_ERR_NOT_FOUND;
}

std::vector<LoScalar> LoDb::select(const char *table_name, LoDbFilter filter, LoDbComparator comparator, size_t limit)
{
    std::vector<LoScalar> results;

    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return results;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not found: %s", table_name);
        return results;
    }

    File dir = LoFS::open(table->table_path, FILE_O_READ);
    if (!dir) {
        LODB_LOG_DEBUG("Table directory not found: %s", table->table_path);
        return results;
    }

    if (!dir.isDirectory()) {
        LODB_LOG_ERROR("Table path is not a directory: %s", table->table_path);
        dir.close();
        return results;
    }

    while (true) {
        File file = dir.openNextFile();
        if (!file) {
            break;
        }

        if (file.isDirectory()) {
            file.close();
            continue;
        }

        std::string pathStr = file.name();
        file.close();

        size_t lastSlash = pathStr.rfind('/');
        std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

        if (!isLsRecordFile(filename)) {
            LODB_LOG_DEBUG("Skipped non-.ls file: %s", filename.c_str());
            continue;
        }

        std::string uuid_hex_str = filename.substr(0, filename.size() - 3);

        lodb_uuid_t uuid;
        uint32_t high, low;
        if (sscanf(uuid_hex_str.c_str(), "%08x%08x", &high, &low) != 2) {
            LODB_LOG_WARN("Failed to parse UUID from filename: %s", uuid_hex_str.c_str());
            continue;
        }
        uuid = ((uint64_t)high << 32) | (uint64_t)low;

        LoScalar record;
        LoDbError err = get(table_name, uuid, record);

        if (err != LODB_OK) {
            LODB_LOG_WARN("Failed to read record " LODB_UUID_FMT " during select", LODB_UUID_ARGS(uuid));
            continue;
        }

        if (filter && !filter(record)) {
            LODB_LOG_DEBUG("Record " LODB_UUID_FMT " filtered out", LODB_UUID_ARGS(uuid));
            continue;
        }

        results.push_back(record);
        LODB_LOG_DEBUG("Added record " LODB_UUID_FMT " to results", LODB_UUID_ARGS(uuid));
    }

    dir.close();

    LODB_LOG_INFO("Select from %s: %u records after filtering", table_name, (unsigned)results.size());

    if (comparator && !results.empty()) {
        std::sort(results.begin(), results.end(), [&comparator](const LoScalar &a, const LoScalar &b) {
            return comparator(a, b) < 0;
        });
        LODB_LOG_DEBUG("Sorted %u records", (unsigned)results.size());
    }

    if (limit > 0 && results.size() > limit) {
        results.resize(limit);
        LODB_LOG_DEBUG("Limited results to %u records", (unsigned)limit);
    }

    LODB_LOG_INFO("Select from %s complete: %u records returned", table_name, (unsigned)results.size());
    return results;
}

int LoDb::count(const char *table_name, LoDbFilter filter)
{
    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return -1;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not found: %s", table_name);
        return -1;
    }

    int cnt = 0;

    if (!filter) {
        File dir = LoFS::open(table->table_path, FILE_O_READ);
        if (!dir) {
            LODB_LOG_DEBUG("Table directory not found: %s", table->table_path);
            return 0;
        }

        if (!dir.isDirectory()) {
            LODB_LOG_ERROR("Table path is not a directory: %s", table->table_path);
            dir.close();
            return -1;
        }

        while (true) {
            File file = dir.openNextFile();
            if (!file) {
                break;
            }

            if (file.isDirectory()) {
                file.close();
                continue;
            }

            std::string pathStr = file.name();
            file.close();

            size_t lastSlash = pathStr.rfind('/');
            std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

            if (isLsRecordFile(filename)) {
                cnt++;
            }
        }

        dir.close();
        LODB_LOG_DEBUG("Counted %d records in %s (no filter)", cnt, table_name);
        return cnt;
    }

    auto results = select(table_name, filter, LoDbComparator(), 0);
    cnt = (int)results.size();

    LODB_LOG_DEBUG("Counted %d records in %s (with filter)", cnt, table_name);
    return cnt;
}

LoDbError LoDb::truncate(const char *table_name)
{
    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not registered: %s", table_name);
        return LODB_ERR_INVALID;
    }

    File dir = LoFS::open(table->table_path, FILE_O_READ);
    if (!dir) {
        LODB_LOG_DEBUG("Table directory not found: %s (already empty)", table->table_path);
        return LODB_OK;
    }

    if (!dir.isDirectory()) {
        LODB_LOG_ERROR("Table path is not a directory: %s", table->table_path);
        dir.close();
        return LODB_ERR_INVALID;
    }

    int deletedCount = 0;
    while (true) {
        File file = dir.openNextFile();
        if (!file) {
            break;
        }

        if (file.isDirectory()) {
            file.close();
            continue;
        }

        std::string pathStr = file.name();
        file.close();

        size_t lastSlash = pathStr.rfind('/');
        std::string filename = (lastSlash != std::string::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

        char file_path[192];
        snprintf(file_path, sizeof(file_path), "%s/%s", table->table_path, filename.c_str());

        if (LoFS::remove(file_path)) {
            deletedCount++;
        } else {
            LODB_LOG_WARN("Failed to delete file during truncate: %s", file_path);
        }
    }

    dir.close();

    LODB_LOG_INFO("Truncated table %s: deleted %d records", table_name, deletedCount);
    return LODB_OK;
}

LoDbError LoDb::drop(const char *table_name)
{
    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return LODB_ERR_INVALID;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not registered: %s", table_name);
        return LODB_ERR_INVALID;
    }

    LoDbError err = truncate(table_name);
    if (err != LODB_OK) {
        LODB_LOG_WARN("Failed to truncate table before drop: %s", table_name);
    }

    if (LoFS::rmdir(table->table_path, true)) {
        LODB_LOG_DEBUG("Removed table directory: %s", table->table_path);
    } else {
        LODB_LOG_WARN("Failed to remove table directory: %s", table->table_path);
    }

    tables.erase(table_name);

    LODB_LOG_INFO("Dropped table: %s", table_name);
    return LODB_OK;
}
