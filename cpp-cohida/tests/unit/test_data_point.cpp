#include <gtest/gtest.h>
#include "models/DataPoint.h"

using namespace models;
using namespace std::chrono;

class DataPointTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Tear down code
    }
};

TEST_F(DataPointTest, CryptoPriceDataValidation) {
    auto now = system_clock::now();
    Decimal price("100.50");
    Decimal volume("1000.00");

    CryptoPriceData valid_data("BTC-USD", now, price, price, price, price, volume);
    EXPECT_TRUE(valid_data.isValid());

    EXPECT_THROW({
        CryptoPriceData invalid_symbol("", now, price, price, price, price, volume);
    }, std::invalid_argument);

    EXPECT_THROW({
        CryptoPriceData invalid_price("BTC-USD", now, Decimal("-10.0"), price, price, price, volume);
    }, std::invalid_argument);

    EXPECT_THROW({
        CryptoPriceData invalid_volume("BTC-USD", now, price, price, price, price, Decimal("-100.0"));
    }, std::invalid_argument);
}

TEST_F(DataPointTest, SymbolInfoValidation) {
    SymbolInfo valid_symbol("BTC-USD", "BTC", "USD", "Bitcoin USD", "online");
    EXPECT_TRUE(valid_symbol.isValid());

    EXPECT_THROW({
        SymbolInfo invalid_symbol("", "BTC", "USD", "Bitcoin USD", "online");
    }, std::invalid_argument);

    EXPECT_THROW({
        SymbolInfo invalid_base("", "", "USD", "Bitcoin USD", "online");
    }, std::invalid_argument);

    SymbolInfo invalid_status("BTC-USD", "BTC", "USD", "Bitcoin USD", "invalid");
    EXPECT_TRUE(invalid_status.isValid());
}

TEST_F(DataPointTest, DataRetrievalRequestValidation) {
    auto now = system_clock::now();
    auto tomorrow = now + hours(24);

    DataRetrievalRequest valid_request("BTC-USD", now, tomorrow, 3600);
    EXPECT_TRUE(valid_request.isValid());

    EXPECT_THROW({
        DataRetrievalRequest invalid_symbol("", now, tomorrow, 3600);
    }, std::invalid_argument);

    EXPECT_THROW({
        DataRetrievalRequest invalid_date("BTC-USD", tomorrow, now, 3600);
    }, std::invalid_argument);

    EXPECT_THROW({
        DataRetrievalRequest invalid_granularity("BTC-USD", now, tomorrow, 123);
    }, std::invalid_argument);
}

TEST_F(DataPointTest, SymbolValidator) {
    EXPECT_TRUE(SymbolValidator::is_valid_symbol("BTC-USD"));
    EXPECT_TRUE(SymbolValidator::is_valid_symbol("ETH-EUR"));
    EXPECT_FALSE(SymbolValidator::is_valid_symbol("BTCUSD"));
    EXPECT_FALSE(SymbolValidator::is_valid_symbol("BTC-"));
    EXPECT_FALSE(SymbolValidator::is_valid_symbol("-USD"));
    EXPECT_FALSE(SymbolValidator::is_valid_symbol("BT-USD"));
    EXPECT_FALSE(SymbolValidator::is_valid_symbol("BITCOIN-USD"));
    EXPECT_FALSE(SymbolValidator::is_valid_symbol("BTC-US"));
}

TEST_F(DataPointTest, SymbolNormalization) {
    EXPECT_EQ(SymbolValidator::normalize_symbol("btc-usd"), "BTC-USD");
    EXPECT_EQ(SymbolValidator::normalize_symbol("  eth-eur  "), "ETH-EUR");
    EXPECT_EQ(SymbolValidator::normalize_symbol("LTC-USD"), "LTC-USD");
}

TEST_F(DataPointTest, DataRetrievalResult) {
    auto now = system_clock::now();
    Decimal price("100.50");
    Decimal volume("1000.00");

    std::vector<CryptoPriceData> data_points;
    data_points.emplace_back("BTC-USD", now, price, price, price, price, volume);

    DataRetrievalResult success_result("BTC-USD", true, data_points);
    EXPECT_TRUE(success_result.success);
    EXPECT_EQ(success_result.data_count(), 1);
    EXPECT_FALSE(success_result.is_empty());

    DataRetrievalResult error_result("BTC-USD", false, {}, "API connection failed");
    EXPECT_FALSE(error_result.success);
    EXPECT_TRUE(error_result.is_empty());
    EXPECT_EQ(error_result.error_message, "API connection failed");
}

