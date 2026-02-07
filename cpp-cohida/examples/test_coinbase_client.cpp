#include <iostream>
#include <chrono>
#include "api/CoinbaseClient.h"
#include "models/DataPoint.h"

using namespace api;
using namespace models;
using namespace std::chrono;

void test_coinbase_client() {
    try {
        std::cout << "Testing Coinbase API Client..." << std::endl;

        CoinbaseClient client;

        std::cout << "1. Test Connection..." << std::endl;
        bool connected = client.test_connection();
        std::cout << "Connected: " << std::boolalpha << connected << std::endl;
        if (!connected) {
            std::cerr << "Connection test failed" << std::endl;
            return;
        }

        std::cout << "\n2. Get Available Symbols..." << std::endl;
        auto symbols = client.get_available_symbols();
        std::cout << "Found " << symbols.size() << " symbols" << std::endl;

        if (!symbols.empty()) {
            std::cout << "First 5 symbols:" << std::endl;
            for (size_t i = 0; i < std::min(symbols.size(), size_t(5)); ++i) {
                std::cout << "  - " << symbols[i].symbol << " (" << symbols[i].display_name << ")" << std::endl;
            }
        }

        std::cout << "\n3. Get Symbol Info (BTC-USD)..." << std::endl;
        auto btc_info = client.get_symbol_info("BTC-USD");
        if (btc_info) {
            std::cout << "Symbol: " << btc_info->symbol << std::endl;
            std::cout << "Base Currency: " << btc_info->base_currency << std::endl;
            std::cout << "Quote Currency: " << btc_info->quote_currency << std::endl;
            std::cout << "Status: " << btc_info->status << std::endl;
        } else {
            std::cout << "BTC-USD symbol info not available" << std::endl;
        }

        std::cout << "\n4. Is Symbol Available (BTC-USD)..." << std::endl;
        bool btc_available = client.is_symbol_available("BTC-USD");
        std::cout << "Available: " << std::boolalpha << btc_available << std::endl;

        std::cout << "\n5. Get Current Price (BTC-USD)..." << std::endl;
        auto btc_price = client.get_current_price("BTC-USD");
        if (btc_price) {
            std::cout << "Price: $" << *btc_price << std::endl;
        } else {
            std::cout << "Failed to get BTC-USD price" << std::endl;
        }

        std::cout << "\n6. Get Historical Candles (BTC-USD)..." << std::endl;
        auto now = system_clock::now();
        auto one_hour_ago = now - hours(1);
        auto candles = client.get_historical_candles("BTC-USD", one_hour_ago, now, 60);

        if (!candles.empty()) {
            std::cout << "Retrieved " << candles.size() << " candles" << std::endl;
            auto first = candles.front();
            auto last = candles.back();
            std::cout << "First candle: " << first.timestamp.time_since_epoch().count() << std::endl;
            std::cout << "Last candle: " << last.timestamp.time_since_epoch().count() << std::endl;
            std::cout << "Price range: $" << first.low_price << " - $" << first.high_price << std::endl;
        } else {
            std::cout << "No candles retrieved for BTC-USD" << std::endl;
        }

        std::cout << "\n7. Symbol Validation..." << std::endl;
        std::vector<std::string> test_symbols = {"BTC-USD", "ETH-EUR", "btc-usd", "invalid-symbol", "BTCUSD"};
        for (const auto& symbol : test_symbols) {
            std::cout << "  " << symbol << ": ";
            if (SymbolValidator::is_valid_symbol(symbol)) {
                std::cout << "Valid (" << SymbolValidator::normalize_symbol(symbol) << ")" << std::endl;
            } else {
                std::cout << "Invalid" << std::endl;
            }
        }

        std::cout << "\nAll tests completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    test_coinbase_client();
    return 0;
}