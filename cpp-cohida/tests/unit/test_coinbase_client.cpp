#include "api/CoinbaseClient.h"
#include "config/Config.h"
#include "utils/Logger.h"
#include <gtest/gtest.h>

using namespace api;
using namespace config;
using namespace utils;

class CoinbaseClientTest : public ::testing::Test {
protected:
  std::unique_ptr<CoinbaseClient> client;

  void SetUp() override {
    // Initialize logger
    utils::Logger::initialize("debug", "test_logs/test_coinbase_client.log");

    // Load test configuration
    config::Config &config = config::Config::get_instance();
    config.load(".env.test");

    client = std::make_unique<CoinbaseClient>(
        config.api_key(), config.api_secret(), config.api_passphrase());
  }

  void TearDown() override { client.reset(); }
};

TEST_F(CoinbaseClientTest, Initialization) { EXPECT_TRUE(client != nullptr); }

TEST_F(CoinbaseClientTest, IsAuthenticated) {
  EXPECT_TRUE(client->is_authenticated());
}

TEST_F(CoinbaseClientTest, SandboxMode) {
  EXPECT_FALSE(client->sandbox_mode());
}

TEST_F(CoinbaseClientTest, TestConnection) {
  bool success = client->test_connection();
  EXPECT_TRUE(success) << "Failed to connect to Coinbase API";
}

TEST_F(CoinbaseClientTest, GetAvailableSymbols) {
  auto symbols = client->get_available_symbols();
  EXPECT_GT(symbols.size(), 0) << "Failed to retrieve available symbols";
}

TEST_F(CoinbaseClientTest, GetSymbolInfo) {
  auto symbol_info = client->get_symbol_info("BTC-USD");
  EXPECT_TRUE(symbol_info.has_value()) << "Failed to get BTC-USD symbol info";

  if (symbol_info) {
    EXPECT_EQ(symbol_info->symbol, "BTC-USD");
    EXPECT_EQ(symbol_info->base_currency, "BTC");
    EXPECT_EQ(symbol_info->quote_currency, "USD");
    EXPECT_FALSE(symbol_info->display_name.empty());
  }
}

TEST_F(CoinbaseClientTest, IsSymbolAvailable) {
  bool available = client->is_symbol_available("BTC-USD");
  EXPECT_TRUE(available) << "BTC-USD should be available";

  bool unavailable = client->is_symbol_available("INVALID-SYMBOL");
  EXPECT_FALSE(unavailable) << "INVALID-SYMBOL should be unavailable";
}

TEST_F(CoinbaseClientTest, GetCurrentPrice) {
  auto price = client->get_current_price("BTC-USD");
  EXPECT_TRUE(price.has_value()) << "Failed to get current BTC-USD price";

  if (price) {
    EXPECT_GT(static_cast<double>(*price), 0.0) << "Price should be positive";
  }
}

TEST_F(CoinbaseClientTest, GetHistoricalCandles) {
  auto now = std::chrono::system_clock::now();
  auto one_day_ago = now - std::chrono::hours(24);
  auto candles =
      client->get_historical_candles("BTC-USD", one_day_ago, now, 3600);

  EXPECT_GT(candles.size(), 0) << "Failed to retrieve historical candles";

  for (const auto &candle : candles) {
    EXPECT_EQ(candle.symbol, "BTC-USD");
    EXPECT_GT(static_cast<double>(candle.close_price), 0.0);
    EXPECT_GT(static_cast<double>(candle.volume), 0.0);
    EXPECT_LE(candle.timestamp, now);
    EXPECT_GE(candle.timestamp, one_day_ago);
  }
}

TEST_F(CoinbaseClientTest, RateLimiter) {
  RateLimiter limiter(10, 2.0);
  int acquired = 0;

  for (int i = 0; i < 15; ++i) {
    if (limiter.try_acquire()) {
      acquired++;
    }
  }

  EXPECT_EQ(acquired, 10) << "Should acquire exactly 10 tokens initially";

  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  bool acquired_after_wait = limiter.try_acquire();
  EXPECT_TRUE(acquired_after_wait) << "Should acquire a token after waiting";
}

TEST_F(CoinbaseClientTest, RetryPolicy) {
  RetryPolicy policy(3, 100);

  EXPECT_TRUE(policy.should_retry(0, ApiException("500 Server Error")));
  EXPECT_TRUE(policy.should_retry(1, ApiException("500 Server Error")));
  EXPECT_FALSE(policy.should_retry(2, ApiException("500 Server Error")));

  EXPECT_FALSE(policy.should_retry(0, ApiException("404 Not Found")));
  EXPECT_FALSE(policy.should_retry(0, ApiException("401 Unauthorized")));

  auto next_time = policy.get_next_attempt_time(0);
  EXPECT_GT(next_time, std::chrono::system_clock::now());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}