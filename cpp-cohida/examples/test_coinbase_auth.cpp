#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include "api/CoinbaseClient.h"
#include "config/Config.h"
#include "utils/Logger.h"

using namespace api;
using namespace config;
using namespace std;
using namespace chrono;

// Helper function to convert time point to string
string time_point_to_string(system_clock::time_point tp) {
    auto time_t_val = system_clock::to_time_t(tp);
    std::tm tm_val;
    gmtime_r(&time_t_val, &tm_val);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int main() {
    // Initialize logger
    utils::Logger::initialize();
    
    // Load configuration
    try {
        Config::get_instance().load();
        LOG_INFO("Configuration loaded successfully");
    } catch (const exception& e) {
        LOG_ERROR("Failed to load configuration: {}", e.what());
        return 1;
    }
    
    // Get API credentials from config
    string apiKey = Config::get_instance().api_key();
    string apiSecret = Config::get_instance().api_secret();
    string apiPassphrase = Config::get_instance().api_passphrase();
    
    // Create Coinbase client
    CoinbaseClient client(apiKey, apiSecret, apiPassphrase);
    
    // Test connection
    LOG_INFO("Testing Coinbase API connection...");
    if (client.test_connection()) {
        LOG_INFO("Connection successful");
    } else {
        LOG_ERROR("Connection failed");
        return 1;
    }
    
    // Check authentication status
    LOG_INFO("Authentication status: {}", client.is_authenticated() ? "Authenticated" : "Not authenticated");
    
    // Test authentication (try to get accounts - requires authentication)
    if (client.is_authenticated()) {
        LOG_INFO("=== Testing Authenticated API ===");
        
        try {
            // This is a placeholder - the actual implementation of get_accounts would need to be added
            LOG_INFO("Note: get_accounts method not yet implemented in this example");
        } catch (const ApiException& ex) {
            LOG_ERROR("API error: {}", ex.what());
        } catch (const exception& ex) {
            LOG_ERROR("Error: {}", ex.what());
        }
    }
    
    // Get available symbols
    LOG_INFO("=== Getting Available Symbols ===");
    try {
        vector<models::SymbolInfo> symbols = client.get_available_symbols();
        LOG_INFO("Found {} symbols", symbols.size());
        
        // Print first 5 symbols
        cout << "First 5 symbols:" << endl;
        for (size_t i = 0; i < min(symbols.size(), static_cast<size_t>(5)); ++i) {
            cout << "- " << symbols[i].symbol << " (" << symbols[i].display_name << ")" << endl;
        }
        
        // Check if BTC-USD is available
        string testSymbol = "BTC-USD";
        bool isAvailable = client.is_symbol_available(testSymbol);
        LOG_INFO("Symbol {} is {}", testSymbol, isAvailable ? "available" : "not available");
        
        // Get symbol info
        if (isAvailable) {
            auto symbolInfo = client.get_symbol_info(testSymbol);
            if (symbolInfo) {
                cout << "\n" << testSymbol << " details:" << endl;
                cout << "  Base currency: " << symbolInfo->base_currency << endl;
                cout << "  Quote currency: " << symbolInfo->quote_currency << endl;
                cout << "  Status: " << symbolInfo->status << endl;
            }
        }
        
    } catch (const ApiException& ex) {
        LOG_ERROR("API error: {}", ex.what());
    } catch (const exception& ex) {
        LOG_ERROR("Error: {}", ex.what());
    }
    
    // Test price retrieval
    LOG_INFO("=== Testing Price Retrieval ===");
    try {
        string symbol = "BTC-USD";
        auto price = client.get_current_price(symbol);
        if (price) {
            LOG_INFO("Current price of {}: {}", symbol, *price);
        } else {
            LOG_WARN("Could not retrieve price for {}", symbol);
        }
    } catch (const ApiException& ex) {
        LOG_ERROR("API error: {}", ex.what());
    } catch (const exception& ex) {
        LOG_ERROR("Error: {}", ex.what());
    }
    
    // Test historical data retrieval
    LOG_INFO("=== Testing Historical Data Retrieval ===");
    try {
        string symbol = "BTC-USD";
        auto end = system_clock::now();
        auto start = end - hours(24); // Last 24 hours
        int granularity = 3600; // 1 hour
        
        auto candles = client.get_historical_candles(symbol, start, end, granularity);
        LOG_INFO("Retrieved {} candles for {}", candles.size(), symbol);
        
        if (!candles.empty()) {
            cout << "First candle:" << endl;
            cout << "  Time: " << time_point_to_string(candles[0].timestamp) << endl;
            cout << "  Open: " << candles[0].open_price << endl;
            cout << "  High: " << candles[0].high_price << endl;
            cout << "  Low: " << candles[0].low_price << endl;
            cout << "  Close: " << candles[0].close_price << endl;
            cout << "  Volume: " << candles[0].volume << endl;
        }
        
    } catch (const ApiException& ex) {
        LOG_ERROR("API error: {}", ex.what());
    } catch (const exception& ex) {
        LOG_ERROR("Error: {}", ex.what());
    }
    
    LOG_INFO("=== Done ===");
    
    return 0;
}
