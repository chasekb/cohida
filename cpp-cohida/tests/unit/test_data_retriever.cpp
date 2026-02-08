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
        // Logger cleanup handled by destructor
    }
};

TEST_F(DataRetrieverTest, TestInitialization) {
    EXPECT_NO_THROW({
        data::DataRetriever retriever;
    });
}

TEST_F(DataRetrieverTest, TestValidateSymbol) {
    data::DataRetriever retriever;
    
    // Test valid symbol
    EXPECT_TRUE(retriever.validate_symbol("BTC-USD"));
    
    // Test invalid symbol
    EXPECT_FALSE(retriever.validate_symbol("INVALID-SYMBOL-XYZ"));
}

TEST_F(DataRetrieverTest, TestRetrieveAllHistoricalData) {
    data::DataRetriever retriever;
    
    // Retrieve last 24 hours of data with 1 hour granularity
    auto result = retriever.retrieve_all_historical_data("BTC-USD", 3600, std::nullopt);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_FALSE(result.data_points.empty());
    }
}

TEST_F(DataRetrieverTest, TestRetrieveHistoricalData) {
    data::DataRetriever retriever;
    
    auto now = std::chrono::system_clock::now();
    auto two_hours_ago = now - std::chrono::hours(2);
    
    // Use the public API that takes individual parameters
    auto result = retriever.retrieve_historical_data("BTC-USD", two_hours_ago, now, 3600);
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_FALSE(result.data_points.empty());
        EXPECT_EQ(result.symbol, "BTC-USD");
    }
}

TEST_F(DataRetrieverTest, TestRetrieveHistoricalDataInvalidSymbol) {
    data::DataRetriever retriever;
    
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    
    auto result = retriever.retrieve_historical_data("INVALID-SYMBOL-XYZ", one_hour_ago, now, 3600);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

