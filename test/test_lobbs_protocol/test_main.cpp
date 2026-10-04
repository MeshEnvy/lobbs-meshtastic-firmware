#include "TestUtil.h"
#include "tests/protocol_tests.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    lobbsRunProtocolTests();
    exit(UNITY_END());
}

void loop() {}
