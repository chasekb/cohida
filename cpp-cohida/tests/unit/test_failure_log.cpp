#include <gtest/gtest.h>
#include <utils/FailureLog.h>

TEST(FailureLogTest, SerializesRequiredFieldsAndRedactsSecrets) {
    const utils::FailureRecord record{
        "2026-09-23T12:34:56Z", "BTC-USD", 3600,
        "2026-09-22T00:00:00Z", "2026-09-22T01:00:00Z",
        "retrieval_chunk", "api_error",
        utils::sanitize_failure_summary("token=secret-value; request failed"),
        "not_retried", "skipped"};

    const auto json = record.to_json();
    EXPECT_EQ(json.at("symbol"), "BTC-USD");
    EXPECT_EQ(json.at("granularity"), 3600);
    EXPECT_EQ(json.at("stage"), "retrieval_chunk");
    EXPECT_EQ(json.at("symbol_outcome"), "skipped");
    EXPECT_EQ(json.at("error_summary"), "token=[REDACTED]; request failed");
    EXPECT_EQ(json.size(), 10U);
}

TEST(FailureLogTest, BoundsAndCleansErrorSummary) {
    const auto summary = utils::sanitize_failure_summary("line\r\n" + std::string(600, 'x'));
    EXPECT_EQ(summary.find('\r'), std::string::npos);
    EXPECT_LE(summary.size(), 512U);
}
