#pragma once

#include <lofs/LoFS.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <pb.h>
#include <string>
#include <vector>

/**
 * LoDB - Synchronous Protobuf Database
 *
 * Filesystem-backed records under a configurable prefix (default `/lodb/...` for
 * compatibility with older installs; use `LoFS::FSType::INTERNAL` or `SD` for explicit
 * `/internal/lodb/...` or `/sd/lodb/...`). I/O goes through LoFS (locking inside LoFS).
 */

#ifndef LODB_VERSION
#define LODB_VERSION "1.4.0"
#endif

#ifndef LODB_LOG_DEBUG
#define LODB_LOG_DEBUG(...) ((void)0)
#endif
#ifndef LODB_LOG_INFO
#define LODB_LOG_INFO(...) ((void)0)
#endif
#ifndef LODB_LOG_WARN
#define LODB_LOG_WARN(...) ((void)0)
#endif
#ifndef LODB_LOG_ERROR
#define LODB_LOG_ERROR(...) ((void)0)
#endif

typedef uint64_t lodb_uuid_t;

#define LODB_UUID_FMT "%08x%08x"
#define LODB_UUID_ARGS(uuid) (uint32_t)((uuid) >> 32), (uint32_t)((uuid)&0xFFFFFFFF)

typedef enum {
    LODB_OK = 0,
    LODB_ERR_NOT_FOUND,
    LODB_ERR_IO,
    LODB_ERR_DECODE,
    LODB_ERR_ENCODE,
    LODB_ERR_INVALID
} LoDbError;

typedef std::function<bool(const void *)> LoDbFilter;
typedef std::function<int(const void *, const void *)> LoDbComparator;

void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17]);
lodb_uuid_t lodb_new_uuid(const char *str, uint64_t salt);

/** Weak by default (`millis()`); override with a strong definition for wall time. */
uint32_t lodb_now_ms(void);

class LoDb
{
  public:
    /**
     * @param db_name Database directory name under the LoDB root for this filesystem choice.
     * @param filesystem AUTO uses legacy `/lodb/...` (internal via LoFS). INTERNAL → `/internal/lodb/...`,
     *        SD → `/sd/lodb/...` when SD is available (otherwise internal with warn).
     */
    LoDb(const char *db_name, LoFS::FSType filesystem = LoFS::FSType::AUTO);
    ~LoDb();

    LoDbError registerTable(const char *table_name, const pb_msgdesc_t *pb_descriptor, size_t record_size);
    LoDbError insert(const char *table_name, lodb_uuid_t uuid, const void *record);
    LoDbError get(const char *table_name, lodb_uuid_t uuid, void *record_out);
    LoDbError update(const char *table_name, lodb_uuid_t uuid, const void *record);
    LoDbError deleteRecord(const char *table_name, lodb_uuid_t uuid);
    std::vector<void *> select(const char *table_name, LoDbFilter filter = LoDbFilter(),
                               LoDbComparator comparator = LoDbComparator(), size_t limit = 0);
    static void freeRecords(std::vector<void *> &records);
    int count(const char *table_name, LoDbFilter filter = LoDbFilter());
    LoDbError truncate(const char *table_name);
    LoDbError drop(const char *table_name);

  private:
    struct TableMetadata {
        std::string table_name;
        const pb_msgdesc_t *pb_descriptor;
        size_t record_size;
        char table_path[160];
    };

    std::string db_name;
    char db_path[128];
    std::map<std::string, TableMetadata> tables;

    TableMetadata *getTable(const char *table_name);
};
