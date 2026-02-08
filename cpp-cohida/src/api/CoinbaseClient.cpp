#include "api/CoinbaseClient.h"
#include "config/Config.h"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace api {

// Helper function to convert time_point to ISO string
std::string time_point_to_iso_string(system_clock::time_point tp) {
    auto time_t_val = system_clock::to_time_t(tp);
    std::tm tm_val;
    gmtime_r(&time_t_val, &tm_val);

    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

class CoinbaseClient::Impl {
public:
    Impl(const std::string& apiKey, const std::string& apiSecret, const std::string& apiPassphrase) 
        : apiKey_(apiKey), apiSecret_(apiSecret), apiPassphrase_(apiPassphrase), curl_initialized_(false) {
        // Defer curl_global_init until first use to avoid issues when Config is not loaded
    }

    ~Impl() {
        if (curl_initialized_) {
            curl_global_cleanup();
        }
    }

    void ensure_curl_initialized() {
        if (!curl_initialized_) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl_initialized_ = true;
        }
    }

    std::string generateSignature(const std::string& timestamp, const std::string& method,
                                 const std::string& path, const std::string& body = "") {
        std::string prehash = timestamp + method + path + body;
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        
        // Create HMAC-SHA256 using OpenSSL 3.0 API
        EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
        OSSL_PARAM params[2];
        const EVP_MD* md = EVP_sha256();
        params[0] = OSSL_PARAM_construct_utf8_string("digest", (char*)EVP_MD_get0_name(md), 0);
        params[1] = OSSL_PARAM_construct_end();
        
        EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
        EVP_MAC_init(ctx, reinterpret_cast<const unsigned char*>(apiSecret_.c_str()), apiSecret_.size(), params);
        EVP_MAC_update(ctx, reinterpret_cast<const unsigned char*>(prehash.c_str()), prehash.size());
        size_t len;
        EVP_MAC_final(ctx, hash, &len, EVP_MAX_MD_SIZE);
        hash_len = static_cast<unsigned int>(len);
        
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        
        // Base64 encode
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO* bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);
        
        BIO_write(bio, hash, hash_len);
        BIO_flush(bio);
        
        char* buffer;
        long size = BIO_get_mem_data(bio, &buffer);
        
        std::string signature(buffer, size);
        BIO_free_all(bio);
        
        return signature;
    }

    std::string getTimestamp() {
        auto now = system_clock::now();
        auto duration = duration_cast<milliseconds>(now.time_since_epoch());
        return std::to_string(duration.count() / 1000.0);
    }

    std::string apiKey_;
    std::string apiSecret_;
    std::string apiPassphrase_;
    bool curl_initialized_;

    CURL* create_curl() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw ApiException("Failed to initialize cURL");
        }
        return curl;
    }

    std::string make_request(const std::string& url, const std::string& method = "GET", 
                            const std::string& body = "") {
        ensure_curl_initialized();
        CURL* curl = create_curl();
        std::string response;

        // Extract path from URL (everything after domain)
        size_t pathStart = url.find_first_of('/', url.find("://") + 3);
        std::string path = pathStart != std::string::npos ? url.substr(pathStart) : "/";

        struct curl_slist* headers = nullptr;

        // Add authentication headers if credentials are provided
        if (!apiKey_.empty() && !apiSecret_.empty()) {
            std::string timestamp = getTimestamp();
            std::string signature = generateSignature(timestamp, method, path, body);
            
            headers = curl_slist_append(headers, ("CB-ACCESS-KEY: " + apiKey_).c_str());
            headers = curl_slist_append(headers, ("CB-ACCESS-SIGN: " + signature).c_str());
            headers = curl_slist_append(headers, ("CB-ACCESS-TIMESTAMP: " + timestamp).c_str());
            if (!apiPassphrase_.empty()) {
                headers = curl_slist_append(headers, ("CB-ACCESS-PASSPHRASE: " + apiPassphrase_).c_str());
            }
        }

        // Set content type for POST/PUT requests with body
        if (!body.empty()) {
            headers = curl_slist_append(headers, "Content-Type: application/json");
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        // Set request method and body
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            }
        } else if (method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            }
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }

        // Set headers
        if (headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        CURLcode res = curl_easy_perform(curl);
        long status_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

        if (headers) {
            curl_slist_free_all(headers);
        }

        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw ApiException("cURL request failed: " + std::string(curl_easy_strerror(res)));
        }

        if (status_code >= 400) {
            curl_easy_cleanup(curl);
            throw ApiException("HTTP error: " + std::to_string(status_code) + ", Response: " + response);
        }

        curl_easy_cleanup(curl);
        return response;
    }

    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string get_api_base_url() {
        return config::Config::get_instance().sandbox_mode() ?
            "https://api-public.sandbox.pro.coinbase.com" :
            "https://api.pro.coinbase.com";
    }
};

CoinbaseClient::CoinbaseClient(const std::string& apiKey, const std::string& apiSecret, 
                               const std::string& apiPassphrase) 
    : pImpl(std::make_unique<Impl>(apiKey, apiSecret, apiPassphrase)) {}

CoinbaseClient::~CoinbaseClient() = default;

bool CoinbaseClient::test_connection() {
    try {
        std::string url = pImpl->get_api_base_url() + "/time";
        std::string response = pImpl->make_request(url);
        auto json_response = json::parse(response);

        if (json_response.contains("epoch")) {
            LOG_INFO("Coinbase API connection test successful");
            return true;
        }
        LOG_WARN("Coinbase API connection test returned empty response");
        return false;
    } catch (const std::exception& ex) {
        LOG_ERROR("Coinbase API connection test failed: {}", ex.what());
        return false;
    }
}

std::vector<models::SymbolInfo> CoinbaseClient::get_available_symbols() {
    try {
        std::string url = pImpl->get_api_base_url() + "/products";
        std::string response = pImpl->make_request(url);
        auto products = json::parse(response);

        std::vector<models::SymbolInfo> symbols;
        for (const auto& product : products) {
            if (product.contains("id") && product.contains("base_currency") &&
                product.contains("quote_currency") && product.contains("display_name") &&
                product.contains("status")) {
                symbols.emplace_back(
                    product.at("id").get<std::string>(),
                    product.at("base_currency").get<std::string>(),
                    product.at("quote_currency").get<std::string>(),
                    product.at("display_name").get<std::string>(),
                    product.at("status").get<std::string>()
                );
            }
        }

        LOG_INFO("Retrieved {} available symbols from Coinbase", symbols.size());
        return symbols;
    } catch (const std::exception& ex) {
        LOG_ERROR("Failed to get available symbols: {}", ex.what());
        return {};
    }
}

std::optional<models::SymbolInfo> CoinbaseClient::get_symbol_info(const std::string& symbol) {
    try {
        std::string url = pImpl->get_api_base_url() + "/products/" + symbol;
        std::string response = pImpl->make_request(url);
        auto product = json::parse(response);

        if (product.contains("id") && product.contains("base_currency") &&
            product.contains("quote_currency") && product.contains("display_name") &&
            product.contains("status")) {
            LOG_DEBUG("Retrieved symbol info for {}", symbol);
            return models::SymbolInfo(
                product.at("id").get<std::string>(),
                product.at("base_currency").get<std::string>(),
                product.at("quote_currency").get<std::string>(),
                product.at("display_name").get<std::string>(),
                product.at("status").get<std::string>()
            );
        }

        LOG_WARN("No information found for symbol {}", symbol);
        return std::nullopt;
    } catch (const std::exception& ex) {
        LOG_ERROR("Failed to get symbol info for {}: {}", symbol, ex.what());
        return std::nullopt;
    }
}

std::optional<models::Decimal> CoinbaseClient::get_current_price(const std::string& symbol) {
    try {
        auto start = system_clock::now() - hours(1);
        auto end = system_clock::now();
        auto candles = get_historical_candles(symbol, start, end, 3600);

        if (!candles.empty()) {
            auto price = candles.back().close_price;
            std::ostringstream oss;
            oss << price;
            LOG_DEBUG("Current price for {}: {}", symbol, oss.str());
            return price;
        }

        LOG_WARN("No price data found for {}", symbol);
        return std::nullopt;
    } catch (const std::exception& ex) {
        LOG_ERROR("Failed to get current price for {}: {}", symbol, ex.what());
        return std::nullopt;
    }
}

std::vector<models::CryptoPriceData> CoinbaseClient::get_historical_candles(
    const std::string& symbol,
    system_clock::time_point start,
    system_clock::time_point end,
    int granularity
) {
    try {
        std::string url = pImpl->get_api_base_url() + "/products/" + symbol + "/candles?" +
                        "start=" + time_point_to_iso_string(start) +
                        "&end=" + time_point_to_iso_string(end) +
                        "&granularity=" + std::to_string(granularity);

        std::string response = pImpl->make_request(url);
        auto candles = json::parse(response);

        std::vector<models::CryptoPriceData> data_points;
        for (const auto& candle : candles) {
            if (candle.size() >= 6) {
                auto timestamp = system_clock::time_point(seconds(candle[0].get<long long>()));
                data_points.emplace_back(
                    symbol,
                    timestamp,
                    models::Decimal(candle[3].get<std::string>()),
                    models::Decimal(candle[2].get<std::string>()),
                    models::Decimal(candle[1].get<std::string>()),
                    models::Decimal(candle[4].get<std::string>()),
                    models::Decimal(candle[5].get<std::string>())
                );
            }
        }

        LOG_DEBUG("Retrieved {} candles for {}", data_points.size(), symbol);
        return data_points;
    } catch (const std::exception& ex) {
        LOG_ERROR("Failed to get historical candles for {}: {}", symbol, ex.what());
        return {};
    }
}

bool CoinbaseClient::is_symbol_available(const std::string& symbol) {
    auto symbol_info = get_symbol_info(symbol);
    if (symbol_info) {
        return symbol_info->status == "online";
    }
    return false;
}

bool CoinbaseClient::is_authenticated() const {
    return config::Config::get_instance().api_credentials_valid();
}

bool CoinbaseClient::sandbox_mode() const {
    return config::Config::get_instance().sandbox_mode();
}

RateLimiter::RateLimiter(int max_tokens, double refill_rate)
    : max_tokens_(max_tokens),
      refill_rate_(refill_rate),
      current_tokens_(max_tokens),
      last_refill_(system_clock::now()) {}

double RateLimiter::get_available_tokens() {
    auto now = system_clock::now();
    auto duration = duration_cast<milliseconds>(now - last_refill_).count() / 1000.0;

    double tokens_to_add = duration * refill_rate_;
    current_tokens_ = std::min(static_cast<double>(max_tokens_), current_tokens_ + tokens_to_add);
    last_refill_ = now;

    return current_tokens_;
}

bool RateLimiter::try_acquire() {
    if (get_available_tokens() >= 1.0) {
        current_tokens_ -= 1.0;
        return true;
    }
    return false;
}

void RateLimiter::wait_for_token() {
    while (!try_acquire()) {
        std::this_thread::sleep_for(milliseconds(100));
    }
}

RetryPolicy::RetryPolicy(int max_attempts, int base_delay_ms)
    : max_attempts_(max_attempts),
      base_delay_ms_(base_delay_ms) {}

bool RetryPolicy::should_retry(int attempt, const std::exception& ex) const {
    if (attempt >= max_attempts_ - 1) {
        return false;
    }

    const std::string& ex_msg = ex.what();
    if (ex_msg.find("400") != std::string::npos || ex_msg.find("401") != std::string::npos ||
        ex_msg.find("403") != std::string::npos || ex_msg.find("404") != std::string::npos) {
        return false;
    }

    return true;
}

system_clock::time_point RetryPolicy::get_next_attempt_time(int attempt) const {
    int delay_ms = base_delay_ms_ * (1 << attempt);
    delay_ms = std::min(delay_ms, 10000);
    return system_clock::now() + milliseconds(delay_ms);
}

} // namespace api