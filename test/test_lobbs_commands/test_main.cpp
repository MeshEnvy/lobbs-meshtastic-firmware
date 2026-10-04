#include "TestUtil.h"
#include "tests/command_tests.h"
#include <unity.h>

void setUp(void)
{
    lobbsTestFixtureSetUp();
}

void tearDown(void)
{
    lobbsTestFixtureTearDown();
}

void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    lobbsRunCommandTests();
    exit(UNITY_END());
}

void loop() {}
