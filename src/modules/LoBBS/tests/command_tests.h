#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS && LOBBS_SEED

#include "LoBBSDispatch.h"
#include "LoBBSModule.h"
#include "LoBBSReplyCache.h"
#include "LoBBSSeed.h"
#include "gps/RTC.h"
#include "lofs/LoFS.h"
#include "mesh/NodeDB.h"
#include <cstring>
#include <memory>
#include <string>
#include <unity.h>
#include <vector>

static constexpr uint32_t kLobbsTestBbsNode = 0xBB500001;
static constexpr uint32_t kLobbsTestClientNode = 0xABCD0001;

class LobbsTestNodeDB : public NodeDB
{
  public:
    LobbsTestNodeDB() { myNodeInfo.my_node_num = kLobbsTestBbsNode; }
};

static std::unique_ptr<LobbsTestNodeDB> lobbsTestNodeDb;
static std::unique_ptr<LoBBSModule> lobbsTestModule;
static std::vector<std::string> lobbsTestReplies;

static void lobbsTestSendLine(const char *line)
{
    meshtastic_MeshPacket mp = {};
    mp.from = kLobbsTestClientNode;
    mp.to = kLobbsTestBbsNode;
    size_t len = strlen(line);
    if (len > sizeof(mp.decoded.payload.bytes))
        len = sizeof(mp.decoded.payload.bytes);
    memcpy(mp.decoded.payload.bytes, line, len);
    mp.decoded.payload.size = len;
    lobbsDispatchReceived(lobbsTestModule.get(), mp);
}

static const char *lobbsTestLastReply()
{
    if (lobbsTestReplies.empty())
        return "";
    return lobbsTestReplies.back().c_str();
}

static void lobbsTestFixtureSetUp()
{
    resetRTCStateForTests();
    struct timeval tv = {1000, 0};
    setRTCSystemTimeForTests(&tv);
    lobbsTestNodeDb = std::make_unique<LobbsTestNodeDB>();
    nodeDB = lobbsTestNodeDb.get();
    LoFS::rmdir("/lodb/lobbs", true);
    lobbsTestReplies.clear();
    lobbsTestReplySink = &lobbsTestReplies;
    lobbsTestModule = std::make_unique<LoBBSModule>();
    lobbsSeedAll(*lobbsTestModule);
}

static void lobbsTestFixtureTearDown()
{
    lobbsTestReplySink = nullptr;
    lobbsTestReplies.clear();
    lobbsTestModule.reset();
    lobbsTestNodeDb.reset();
    nodeDB = nullptr;
    resetRTCStateForTests();
}

static void test_command_mail_list_and_plain_page2()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 1/"));
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 2/"));
}

static void test_command_machine_page_from_cache()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/42 mail list");
    const char *p1 = lobbsTestLastReply();
    TEST_ASSERT_NOT_NULL(strstr(p1, "<42:"));
    TEST_ASSERT_NOT_NULL(strstr(p1, "ok"));
    lobbsTestSendLine("/43 p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "<43:"));
}

static void test_command_no_such_page()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    lobbsTestSendLine("/p99");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such page."));
}

static void test_command_new_command_replaces_cache()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    lobbsTestSendLine("/time");
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such page."));
}

static void test_command_error_keeps_cache()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    lobbsTestSendLine("/mail read 99999");
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 2/"));
}

static void test_command_cache_expires()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    struct timeval tv = {1000 + LOBBS_REPLY_CACHE_TTL_SEC + 1, 0};
    setRTCSystemTimeForTests(&tv);
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No cached reply."));
}

static void test_command_users_kick()
{
    lobbsTestSendLine("/login demo03 demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/users kick demo03");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Sessions cleared."));
}

static void test_command_subcommand_with_args()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/news read 1");
    TEST_ASSERT_NULL(strstr(lobbsTestLastReply(), "Unknown command"));
    lobbsTestSendLine("/mail send demo01 hello there");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Mail sent."));
}

static void test_command_help_catalog_and_topics()
{
    lobbsTestSendLine("/help");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), " Help\n"));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "pN (page N of the last reply)"));
    lobbsTestSendLine("/help bogus");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No help found for bogus"));
    lobbsTestSendLine("/help news read");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "read N"));
    TEST_ASSERT_NULL(strstr(lobbsTestLastReply(), "post message"));
}

inline void lobbsRunCommandTests()
{
    RUN_TEST(test_command_mail_list_and_plain_page2);
    RUN_TEST(test_command_machine_page_from_cache);
    RUN_TEST(test_command_no_such_page);
    RUN_TEST(test_command_new_command_replaces_cache);
    RUN_TEST(test_command_error_keeps_cache);
    RUN_TEST(test_command_cache_expires);
    RUN_TEST(test_command_users_kick);
    RUN_TEST(test_command_subcommand_with_args);
    RUN_TEST(test_command_help_catalog_and_topics);
}

#endif
