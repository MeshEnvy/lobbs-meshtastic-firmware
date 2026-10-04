#include <algorithm>
#include <cstring>
#include <lodb/LoDB.h>
#include <lodb/LoDBDiagRecords.h>
#include <loscalar/LoScalar.h>
#include <string>

static LoScalar lodbDiagMake(uint32_t counter, const char *value, bool active)
{
    LoScalar r;
    r.setUint32(LoDbDiagField::FIELD_COUNTER, counter);
    if (value && value[0])
        r.setString(LODB_F_DESCRIPTION, value);
    r.setBool(LoDbDiagField::FIELD_ACTIVE, active);
    return r;
}

static void lodbDiagRead(const LoScalar &r, uint32_t &counter, char *value, size_t valueCap, bool &active)
{
    counter = 0;
    active = false;
    if (valueCap)
        value[0] = '\0';
    r.getUint32(LoDbDiagField::FIELD_COUNTER, counter);
    std::string s;
    if (r.getString(LODB_F_DESCRIPTION, s) && valueCap) {
        strncpy(value, s.c_str(), valueCap - 1);
        value[valueCap - 1] = '\0';
    }
    r.getBool(LoDbDiagField::FIELD_ACTIVE, active);
}

void lodb_diagnostics()
{
    LODB_LOG_INFO("=== LoDB Comprehensive Test Suite ===");
    LODB_LOG_INFO("");

    const char *cleanupDirs[] = {
        "/lodb/test_db_1",          "/lodb/test_db_2",          "/lodb/test_db_3",          "/lodb/test_db_4",
        "/sd/lodb/test_db_1",       "/sd/lodb/test_db_2",       "/sd/lodb/test_db_3",       "/sd/lodb/test_db_4",
        "/internal/lodb/test_db_1", "/internal/lodb/test_db_2", "/internal/lodb/test_db_3", "/internal/lodb/test_db_4",
    };
    size_t numCleanupDirs = sizeof(cleanupDirs) / sizeof(cleanupDirs[0]);
    for (size_t i = 0; i < numCleanupDirs; i++) {
        LoFS::rmdir(cleanupDirs[i], true);
    }

    LODB_LOG_INFO("--- Test 1: Filesystem Availability and Database Initialization ---");
    bool sdAvailable = LoFS::isSDCardAvailable();
    LODB_LOG_INFO("SD Card available: %s", sdAvailable ? "YES" : "NO");
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 2: Create Multiple Databases ---");
    LoDb *db1 = new LoDb("test_db_1");
    LoDb *db2 = new LoDb("test_db_2", LoFS::FSType::INTERNAL);
    LoDb *db3 = sdAvailable ? new LoDb("test_db_3", LoFS::FSType::SD) : nullptr;
    LoDb *db4 = !sdAvailable ? new LoDb("test_db_4", LoFS::FSType::SD) : nullptr;
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 3: Register Tables ---");
    LoDbError err;
    err = db1->registerTable("users");
    LODB_LOG_INFO("db1 users: %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    err = db1->registerTable("messages");
    err = db1->registerTable("logs");
    err = db2->registerTable("items");
    err = db2->registerTable("orders");
    if (db3)
        err = db3->registerTable("events");
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 4: Insert ---");
    lodb_uuid_t uuid;
    LoScalar record = lodbDiagMake(1, "test_value_1", true);
    uuid = lodb_new_uuid(nullptr, 0);
    err = db1->insert("users", uuid, record);
    LODB_LOG_INFO("insert users: %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    lodb_uuid_t uuid1 = uuid;

    record = lodbDiagMake(2, "test_value_2", false);
    uuid = lodb_new_uuid("custom_string_1", 12345);
    err = db1->insert("users", uuid, record);
    lodb_uuid_t uuid2 = uuid;

    record = lodbDiagMake(3, "duplicate", true);
    err = db1->insert("users", uuid1, record);
    LODB_LOG_INFO("duplicate insert: %s (expect FAILED)", err == LODB_OK ? "SUCCESS" : "FAILED");

    record = lodbDiagMake(10, "message_1", true);
    uuid = lodb_new_uuid(nullptr, 0);
    err = db1->insert("messages", uuid, record);
    lodb_uuid_t uuid3 = uuid;

    record = lodbDiagMake(100, "item_1", true);
    uuid = lodb_new_uuid(nullptr, 0);
    err = db2->insert("items", uuid, record);
    lodb_uuid_t uuid4 = uuid;

    for (int i = 0; i < 5; i++) {
        char value[32];
        snprintf(value, sizeof(value), "bulk_test_%d", i);
        record = lodbDiagMake((uint32_t)(20 + i), value, i % 2 == 0);
        uuid = lodb_new_uuid(nullptr, (uint64_t)i);
        db1->insert("users", uuid, record);
    }
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 4b: Stamp created/updated ---");
    err = db1->get("users", uuid1, record);
    uint32_t createdAfterInsert = 0;
    uint32_t updatedAfterInsert = 0;
    if (err == LODB_OK) {
        record.getUint32(LODB_F_CREATED, createdAfterInsert);
        record.getUint32(LODB_F_UPDATED, updatedAfterInsert);
    }
    LODB_LOG_INFO("after insert created=%u updated=%u (expect both set)", createdAfterInsert, updatedAfterInsert);

    LoScalar explicitTs = lodbDiagMake(1, "explicit_ts", true);
    explicitTs.setUint32(LODB_F_CREATED, 42);
    explicitTs.setUint32(LODB_F_UPDATED, 43);
    lodb_uuid_t uuidExplicit = lodb_new_uuid("explicit_ts", 0);
    db1->insert("users", uuidExplicit, explicitTs);
    LoScalar gotExplicit;
    err = db1->get("users", uuidExplicit, gotExplicit);
    uint32_t cExp = 0, uExp = 0;
    if (err == LODB_OK) {
        gotExplicit.getUint32(LODB_F_CREATED, cExp);
        gotExplicit.getUint32(LODB_F_UPDATED, uExp);
    }
    LODB_LOG_INFO("explicit timestamps kept: %s (c=%u u=%u)", (cExp == 42 && uExp == 43) ? "YES" : "NO", cExp, uExp);
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 5: Get ---");
    LoScalar retrieved;
    err = db1->get("users", uuid1, retrieved);
    LODB_LOG_INFO("get users uuid1: %s", err == LODB_OK ? "SUCCESS" : "FAILED");
    if (err == LODB_OK) {
        char val[64];
        uint32_t id = 0;
        bool active = false;
        lodbDiagRead(retrieved, id, val, sizeof(val), active);
        LODB_LOG_INFO("  id=%u value=\"%s\" active=%s", id, val, active ? "true" : "false");
    }

    lodb_uuid_t fakeUuid = lodb_new_uuid("nonexistent", 99999);
    err = db1->get("users", fakeUuid, retrieved);
    LODB_LOG_INFO("get missing: %s", err == LODB_ERR_NOT_FOUND ? "NOT_FOUND" : "unexpected");

    err = db1->get("messages", uuid3, retrieved);
    err = db2->get("items", uuid4, retrieved);
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 6: Update ---");
    record = lodbDiagMake(999, "updated_value", false);
    record.removeField(LODB_F_CREATED);
    record.removeField(LODB_F_UPDATED);
    err = db1->update("users", uuid1, record);
    err = db1->get("users", uuid1, retrieved);
    uint32_t createdAfterUpdate = 0;
    uint32_t updatedAfterUpdate = 0;
    if (err == LODB_OK) {
        char val[64];
        uint32_t id = 0;
        bool active = false;
        lodbDiagRead(retrieved, id, val, sizeof(val), active);
        retrieved.getUint32(LODB_F_CREATED, createdAfterUpdate);
        retrieved.getUint32(LODB_F_UPDATED, updatedAfterUpdate);
        LODB_LOG_INFO("  after update id=%u value=\"%s\" created=%u updated=%u", id, val, createdAfterUpdate, updatedAfterUpdate);
        LODB_LOG_INFO("  created preserved: %s updated changed: %s", createdAfterUpdate == createdAfterInsert ? "YES" : "NO",
                      updatedAfterUpdate != updatedAfterInsert ? "YES" : "NO");
    }
    err = db1->update("users", fakeUuid, record);
    record = lodbDiagMake(888, "updated_message", true);
    record.removeField(LODB_F_CREATED);
    record.removeField(LODB_F_UPDATED);
    err = db1->update("messages", uuid3, record);
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 7: Select ---");
    auto filterActive = [](const LoScalar &rec) -> bool {
        bool active = false;
        rec.getBool(LoDbDiagField::FIELD_ACTIVE, active);
        return active;
    };
    auto filterId = [](const LoScalar &rec) -> bool {
        uint32_t id = 0;
        rec.getUint32(LoDbDiagField::FIELD_COUNTER, id);
        return id > 20;
    };
    auto comparatorId = [](const LoScalar &a, const LoScalar &b) -> int {
        uint32_t ia = 0, ib = 0;
        a.getUint32(LoDbDiagField::FIELD_COUNTER, ia);
        b.getUint32(LoDbDiagField::FIELD_COUNTER, ib);
        if (ia > ib)
            return -1;
        if (ia < ib)
            return 1;
        return 0;
    };

    auto results1 = db1->select("users", LoDbFilter(), LoDbComparator(), 0);
    LODB_LOG_INFO("select all users: %u", (unsigned)results1.size());
    auto results2 = db1->select("users", filterActive, LoDbComparator(), 0);
    LODB_LOG_INFO("select active: %u", (unsigned)results2.size());
    auto results3 = db1->select("users", filterId, LoDbComparator(), 0);
    auto results4 = db1->select("users", LoDbFilter(), comparatorId, 0);
    auto results5 = db1->select("users", LoDbFilter(), LoDbComparator(), 3);
    auto results6 = db1->select("users", filterActive, comparatorId, 2);
    LODB_LOG_INFO("select limited: %u filter+sort: %u", (unsigned)results5.size(), (unsigned)results6.size());
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 8: Count ---");
    int count1 = db1->count("users");
    int count2 = db1->count("users", filterActive);
    LODB_LOG_INFO("count users=%d active=%d messages=%d", count1, count2, db1->count("messages"));
    LODB_LOG_INFO("count items=%d bad table=%d", db2->count("items"), db1->count("nonexistent"));
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 9: Delete ---");
    err = db1->deleteRecord("users", uuid2);
    err = db1->get("users", uuid2, retrieved);
    LODB_LOG_INFO("after delete get: %s", err == LODB_ERR_NOT_FOUND ? "NOT_FOUND" : "unexpected");
    db1->deleteRecord("messages", uuid3);
    db2->deleteRecord("items", uuid4);
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 10: Truncate / Drop ---");
    for (int i = 0; i < 3; i++) {
        char value[32];
        snprintf(value, sizeof(value), "truncate_%d", i);
        record = lodbDiagMake((uint32_t)(100 + i), value, true);
        uuid = lodb_new_uuid(nullptr, (uint64_t)(1000 + i));
        db1->insert("logs", uuid, record);
    }
    db1->truncate("logs");
    LODB_LOG_INFO("logs after truncate: %d", db1->count("logs"));
    db1->drop("logs");
    err = db1->insert("logs", uuid1, record);
    LODB_LOG_INFO("insert after drop: %s", err == LODB_ERR_INVALID ? "INVALID" : "other");
    db1->registerTable("logs");
    LODB_LOG_INFO("");

    LODB_LOG_INFO("--- Test 11: Cross-database ---");
    db2->registerTable("users");
    record = lodbDiagMake(200, "db2_user", true);
    uuid = lodb_new_uuid(nullptr, 2000);
    db2->insert("users", uuid, record);
    LODB_LOG_INFO("db1 users=%d db2 users=%d", db1->count("users"), db2->count("users"));
    LODB_LOG_INFO("");

    db1->truncate("users");
    db1->truncate("messages");
    db2->truncate("items");
    db2->truncate("orders");
    if (db2->count("users") > 0)
        db2->truncate("users");

    delete db1;
    delete db2;
    if (db3)
        delete db3;
    if (db4)
        delete db4;

    for (size_t i = 0; i < numCleanupDirs; i++) {
        if (LoFS::exists(cleanupDirs[i]))
            LoFS::rmdir(cleanupDirs[i], true);
    }

    LODB_LOG_INFO("=== Test Suite Complete ===");
}
