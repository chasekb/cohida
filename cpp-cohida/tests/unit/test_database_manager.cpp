#include <gtest/gtest.h>
#include <database/DatabaseManager.h>
#include <config/Config.h>
#include <utils/Logger.h>
#include <models/DataPoint.h>

class DatabaseManagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Load test configuration
        config::Config::get_instance().load(".env.test");
        utils::Logger::initialize("test_db_manager.log");
    }
    
    static void TearDownTestSuite() {
        // Logger cleanup handled by destructor
    }

    static models::CryptoPriceData point(const std::string& symbol,
                                         int seconds,
                                         const std::string& price = "50000.0") {
        return models::CryptoPriceData(
            symbol,
            std::chrono::system_clock::time_point{} + std::chrono::seconds(seconds),
            models::Decimal(price.c_str()),
            models::Decimal(price.c_str()),
            models::Decimal(price.c_str()),
            models::Decimal(price.c_str()),
            models::Decimal("100.5"));
    }
};

TEST_F(DatabaseManagerTest, TestConnection) {
    // Test connection
    database::DatabaseManager db_manager;
    EXPECT_TRUE(db_manager.test_connection());
}

TEST_F(DatabaseManagerTest, TestWriteAndReadData) {
    // Create test data using CryptoPriceData
    models::CryptoPriceData data_point(
        "BTC-USD",
        std::chrono::system_clock::now(),
        models::Decimal("50000.0"),
        models::Decimal("50500.0"),
        models::Decimal("49500.0"),
        models::Decimal("50200.0"),
        models::Decimal("100.5")
    );
    
    // Write data
    database::DatabaseManager db_manager;
    int written = db_manager.write_data({data_point});
    EXPECT_EQ(written, 1);
    
    // Read data
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto data = db_manager.read_data("BTC-USD", one_hour_ago, now);
    EXPECT_FALSE(data.empty());
}

TEST_F(DatabaseManagerTest, IsolatesFailedPointAndCommitsFollowingPoints) {
    database::DatabaseManager db_manager;
    auto result = db_manager.write_data_detailed({
        point("MIXED-OK-1", 1),
        point("MIXED-TOO-LARGE", 2, "10000000000.0"),
        point("MIXED-OK-2", 3)
    });

    EXPECT_EQ(result.written_count, 2);
    ASSERT_EQ(result.failures.size(), 1);
    EXPECT_EQ(result.failures.front().symbol, "MIXED-TOO-LARGE");
    EXPECT_FALSE(result.failures.front().error_summary.empty());
    EXPECT_FALSE(db_manager.read_data(
        "MIXED-OK-2",
        std::chrono::system_clock::time_point{} - std::chrono::seconds(1),
        std::chrono::system_clock::time_point{} + std::chrono::seconds(4)).empty());
}

TEST_F(DatabaseManagerTest, ReportsAllFailedPointsWithoutClaimingSuccess) {
    database::DatabaseManager db_manager;
    auto result = db_manager.write_data_detailed({
        point("ALL-TOO-LARGE-1", 11, "10000000000.0"),
        point("ALL-TOO-LARGE-2", 12, "10000000000.0")
    });

    EXPECT_EQ(result.written_count, 0);
    EXPECT_FALSE(result.complete());
    ASSERT_EQ(result.failures.size(), 2);
    EXPECT_EQ(result.failures[0].symbol, "ALL-TOO-LARGE-1");
    EXPECT_EQ(result.failures[1].symbol, "ALL-TOO-LARGE-2");
}

TEST_F(DatabaseManagerTest, TestDataCount) {
    database::DatabaseManager db_manager;
    int count = db_manager.get_data_count("BTC-USD");
    EXPECT_GE(count, 0);
}

TEST_F(DatabaseManagerTest, TestLatestTimestamp) {
    database::DatabaseManager db_manager;
    auto latest = db_manager.get_latest_timestamp("BTC-USD");
    
    if (latest) {
        EXPECT_LE(latest.value(), std::chrono::system_clock::now());
    }
}

TEST(DatabaseManagerGranularityTest, TestGranularityHandling) {
    database::DatabaseManager db_manager_60(60);
    EXPECT_TRUE(db_manager_60.test_connection());
    
    database::DatabaseManager db_manager_3600(3600);
    EXPECT_TRUE(db_manager_3600.test_connection());
}

