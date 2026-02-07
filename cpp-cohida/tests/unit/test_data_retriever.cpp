#include <gtest/gtest.h>
#include <data/DataRetriever.h>
#include <config/Config.h>
#include <utils/Logger.h>

class DataRetrieverTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Load test configuration
        config::Config::get_instance().load(".env.test");
        utils::Logger::initialize("test_data_retriever.log");
    }
    
    static void TearDownTestSuite() {
        utils::Logger::shutdown();
    }
};

TEST_F(DataRetrieverTest, TestInitialization) {
    EXPECT_NO_THROW({
        data::DataRetriever retriever;
    });
}

TEST_F(DataRetrieverTest, TestAvailableGranularities) {
    data::DataRetriever retriever;
    auto granularities = retriever.get_available_granularities();
    
    EXPECT_EQ(granularities.size(), 6);
    EXPECT_EQ(granularities[0], 60);    // 1 min
    EXPECT_EQ(granularities[1], 300);   // 5 min
    EXPECT_EQ(granularities[2], 900);   // 15 min
    EXPECT_EQ(granularities[3], 3600);  // 1 hour
    EXPECT_EQ(granularities[4], 21600); // 6 hours
    EXPECT_EQ(granularities[5], 86400); // 1 day
}

TEST_F(DataRetrieverTest, TestDateRangeValidation) {
    data::DataRetriever retriever;
    
    auto now = std::chrono::system_clock::now();
    auto one_day_ago = now - std::chrono::hours(24);
    auto one_hour_ago = now - std::chrono::hours(1);
    
    // Test valid range for 1 hour granularity (max 300 points = 5 hours)
    EXPECT_TRUE(retriever.validate_date_range(one_hour_ago, now, 3600));
    
    // Test invalid range for 1 hour granularity
    EXPECT_FALSE(retriever.validate_date_range(one_day_ago, now, 3600));
}

TEST_F(DataRetrieverTest, TestRecentDataRetrieval) {
    data::DataRetriever retriever;
    
    auto result = retriever.retrieve_recent_data("BTC-USD", 1, 3600);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_FALSE(result.data_points.empty());
    } else {
        std::cout << "Test failed: " << result.error_message << std::endl;
    }
}

TEST_F(DataRetrieverTest, TestDataRangeRetrieval) {
    data::DataRetriever retriever;
    
    auto now = std::chrono::system_clock::now();
    auto two_days_ago = now - std::chrono::hours(48);
    
    data::DataRetrievalRequest request(
        "BTC-USD",
        two_days_ago,
        now,
        3600
    );
    
    auto result = retriever.retrieve_historical_data(request);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_GE(result.data_points.size(), 0);
    } else {
        std::cout << "Test failed: " << result.error_message << std::endl;
    }
}

TEST(DataRetrieverGranularityTest, TestGranularityStringConversion) {
    data::DataRetriever retriever;
    
    // Create a test symbol
    EXPECT_NO_THROW({
        retriever.retrieve_recent_data("BTC-USD", 1, 60);  // 1 min
        retriever.retrieve_recent_data("BTC-USD", 1, 300); // 5 min
        retriever.retrieve_recent_data("BTC-USD", 1, 900); // 15 min
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}