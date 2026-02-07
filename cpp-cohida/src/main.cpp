#include <iostream>
#include "api/CoinbaseClient.h"
#include "config/Config.h"
#include "utils/Logger.h"

using namespace api;
using namespace config;
using namespace std;

int main() {
    utils::Logger::initialize();
    LOG_INFO("Coinbase API Client starting...");

    // Load configuration from environment variables or .env file
    try {
        Config::get_instance().load();
        LOG_INFO("Configuration loaded successfully");
    } catch (const exception& e) {
        LOG_ERROR("Failed to load configuration: {}", e.what());
        return 1;
    }

    // Create Coinbase client
    CoinbaseClient client;
    
    // Test connection
    LOG_INFO("Testing connection to Coinbase API...");
    bool connected = client.test_connection();
    if (!connected) {
        LOG_ERROR("Failed to connect to Coinbase API");
        return 1;
    }
    LOG_INFO("Successfully connected to Coinbase API");

    // Check if we're in sandbox mode
    LOG_INFO("Sandbox mode: {}", client.sandbox_mode() ? "Yes" : "No");

    // Check authentication status
    LOG_INFO("Authenticated: {}", client.is_authenticated() ? "Yes" : "No");

    // Get available symbols
    LOG_INFO("Getting available symbols...");
    auto symbols = client.get_available_symbols();
    if (symbols.empty()) {
        LOG_WARN("No symbols available");
    } else {
        LOG_INFO("Available symbols: {}", symbols.size());
        
        // Print first 10 symbols
        LOG_INFO("First 10 symbols:");
        for (size_t i = 0; i < min(symbols.size(), static_cast<size_t>(10)); ++i) {
            LOG_INFO("- {}", symbols[i].symbol);
        }
    }

    // Test getting BTC-USD price
    string symbol = "BTC-USD";
    LOG_INFO("Getting current price for {}...", symbol);
    auto price = client.get_current_price(symbol);
    if (price) {
        LOG_INFO("Current price: {}", *price);
    } else {
        LOG_WARN("Failed to get current price for {}", symbol);
    }

    // Test getting historical candles
    auto now = chrono::system_clock::now();
    auto one_day_ago = now - chrono::hours(24);
    LOG_INFO("Getting historical candles for {} (last 24 hours)...", symbol);
    auto candles = client.get_historical_candles(symbol, one_day_ago, now, 3600);
    if (candles.empty()) {
        LOG_WARN("No historical candles available for {}", symbol);
    } else {
        LOG_INFO("Historical candles: {} (1-hour intervals)", candles.size());
    }

    LOG_INFO("Coinbase API Client finished successfully");
    return 0;
}
