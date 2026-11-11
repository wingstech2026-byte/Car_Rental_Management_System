// =============================================================================
// SelfTestTests.cpp - runs the built-in BusinessRuleSelfTest under GoogleTest,
// so every one of its checks must also pass in the automated test run.
// =============================================================================
#include "BusinessRuleSelfTest.h"
#include "TextUtils.h"

#include <gtest/gtest.h>

TEST(BusinessRuleSelfTest, AllChecksPass)
{
    const BusinessRuleSelfTest selfTest{};
    const std::vector<SelfTestResult> results = selfTest.run();
    EXPECT_GE(results.size(), 50u);
    for (const SelfTestResult& result : results)
    {
        EXPECT_TRUE(result.passed) << TextUtils::toUtf8(result.name) << ": " << TextUtils::toUtf8(result.detail);
    }
}
