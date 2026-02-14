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

