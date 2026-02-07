#ifndef COINBASE_CLIENT_H
#define COINBASE_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>
#include "ApiException.h"
#include "utils/Logger.h"
#include "models/DataPoint.h"

namespace api {

using namespace std::chrono;
using json = nlohmann::json;

class CoinbaseClient {
public:
    /**
     * @brief Initialize the client with optional API key, secret, and passphrase
     * @param apiKey Optional API key for authentication
     * @param apiSecret Optional API secret for authentication
     * @param apiPassphrase Optional API passphrase for authentication (for Coinbase Pro)
     */
    explicit CoinbaseClient(const std::string& apiKey = "", const std::string& apiSecret = "", 
                           const std::string& apiPassphrase = "");
    ~CoinbaseClient();

    bool test_connection();
    std::vector<models::SymbolInfo> get_available_symbols();
    std::optional<models::SymbolInfo> get_symbol_info(const std::string& symbol);
    std::optional<models::Decimal> get_current_price(const std::string& symbol);
    std::vector<models::CryptoPriceData> get_historical_candles(
        const std::string& symbol,
        system_clock::time_point start,
        system_clock::time_point end,
        int granularity
    );
    bool is_symbol_available(const std::string& symbol);

    bool is_authenticated() const;
    bool sandbox_mode() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

class RateLimiter {
public:
    RateLimiter(int max_tokens, double refill_rate);
    bool try_acquire();
    void wait_for_token();

private:
    int max_tokens_;
    double refill_rate_;
    double current_tokens_;
    system_clock::time_point last_refill_;

    double get_available_tokens();
};

class RetryPolicy {
public:
    RetryPolicy(int max_attempts, int base_delay_ms);
    bool should_retry(int attempt, const std::exception& ex) const;
    system_clock::time_point get_next_attempt_time(int attempt) const;

private:
    int max_attempts_;
    int base_delay_ms_;
};

template<typename T>
std::optional<T> retry_operation(const std::function<std::optional<T>()>& operation,
                               const RetryPolicy& retry_policy) {
    for (int attempt = 0; attempt < retry_policy.max_attempts_; ++attempt) {
        try {
            auto result = operation();
            if (result) {
                return result;
            }
        } catch (const ApiException& ex) {
            if (!retry_policy.should_retry(attempt, ex)) {
                LOG_ERROR("Operation failed after %d attempts: %s", (attempt + 1), ex.what());
                return std::nullopt;
            }
            LOG_WARN("Operation failed on attempt %d: %s", (attempt + 1), ex.what());
            std::this_thread::sleep_until(retry_policy.get_next_attempt_time(attempt));
        } catch (const std::exception& ex) {
            LOG_ERROR("Unhandled exception: %s", ex.what());
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace api

#endif // COINBASE_CLIENT_H