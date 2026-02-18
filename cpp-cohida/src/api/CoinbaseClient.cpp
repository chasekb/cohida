#include "api/CoinbaseClient.h"
#include "config/Config.h"
#include <curl/curl.h>
#include <iomanip>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <random>
#include <sstream>

namespace api {

// Helper function to convert time_point to ISO string
std::string time_point_to_iso_string(std::chrono::system_clock::time_point tp) {
  auto time_t_val = std::chrono::system_clock::to_time_t(tp);
  std::tm tm_val;
  gmtime_r(&time_t_val, &tm_val);

  std::ostringstream oss;
  oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// Helper function to generate random nonce
std::string generate_nonce() {
  static std::random_device rd;
  static std::mt19937_64 gen(rd());
  static std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);
  std::stringstream ss;
  ss << std::hex << dis(gen);
  return ss.str();
}

class CoinbaseClient::Impl {
public:
  Impl(const std::string &apiKey, const std::string &apiSecret,
       const std::string &apiPassphrase)
      : apiKey_(apiKey), apiSecret_(apiSecret), apiPassphrase_(apiPassphrase),
        curl_initialized_(false) {
    // Defer curl_global_init until first use to avoid issues when Config is not
    // loaded
  }

  ~Impl() {
    if (curl_initialized_) {
      curl_global_cleanup();
    }
  }

  void ensure_curl_initialized() {
    if (!curl_initialized_) {
      try {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized_ = true;
      } catch (...) {
        // Ignore curl init errors
      }
    }
  }

  std::string generateJWT(const std::string &method, const std::string &host,
                          const std::string &path) {
    // Format URI as "GET api.coinbase.com/api/v3/brokerage/products"
    std::string uri = method + " " + host + path;

    auto now = std::chrono::system_clock::now();
    auto nbf =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count() -
        10;               // 10s buffer for clock skew
    auto exp = nbf + 120; // 2 minutes expiration

    // Ensure the secret key has proper newlines
    std::string context_secret = apiSecret_;
    size_t pos = 0;
    while ((pos = context_secret.find("\\n", pos)) != std::string::npos) {
      context_secret.replace(pos, 2, "\n");
      pos += 1;
    }

    // Create JWT token
    auto token = jwt::create()
                     .set_subject(apiKey_)
                     .set_issuer("cdp")
                     .set_not_before(std::chrono::system_clock::time_point(
                         std::chrono::seconds{nbf}))
                     .set_expires_at(std::chrono::system_clock::time_point(
                         std::chrono::seconds{exp}))
                     .set_payload_claim("uri", jwt::claim(uri))
                     .set_header_claim("kid", jwt::claim(apiKey_))
                     .set_header_claim("nonce", jwt::claim(generate_nonce()))
                     .sign(jwt::algorithm::es256("", context_secret));

    return token;
  }

  std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return std::to_string(duration.count() / 1000.0);
  }

  std::string apiKey_;
  std::string apiSecret_;
  std::string apiPassphrase_;
  bool curl_initialized_;

  CURL *create_curl() {
    CURL *curl = curl_easy_init();
    if (!curl) {
      throw ApiException("Failed to initialize cURL");
    }
    return curl;
  }

  std::string make_request(const std::string &url,
                           const std::string &method = "GET",
                           const std::string &body = "") {
    ensure_curl_initialized();
    CURL *curl = create_curl();
    std::string response;

    // Extract host and path from URL
    size_t protocolEnd = url.find("://");
    size_t hostStart = protocolEnd == std::string::npos ? 0 : protocolEnd + 3;
    size_t pathStart = url.find('/', hostStart);

    std::string host = pathStart != std::string::npos
                           ? url.substr(hostStart, pathStart - hostStart)
                           : url.substr(hostStart);

    std::string full_path =
        pathStart != std::string::npos ? url.substr(pathStart) : "/";

    // Strip query parameters for JWT claim
    std::string jwt_path = full_path;
    size_t queryStart = jwt_path.find('?');
    if (queryStart != std::string::npos) {
      jwt_path = jwt_path.substr(0, queryStart);
    }

    struct curl_slist *headers = nullptr;

    // Set User-Agent header (required by Coinbase API)
    headers = curl_slist_append(headers, "User-Agent: Cohida/1.0.0");

    // Add authentication headers if credentials are provided
    if (!apiKey_.empty() && !apiSecret_.empty()) {
      try {
        std::string jwt_token = generateJWT(method, host, jwt_path);
        headers = curl_slist_append(
            headers, ("Authorization: Bearer " + jwt_token).c_str());
      } catch (const std::exception &ex) {
        LOG_ERROR("Failed to generate JWT token: {}", ex.what());
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
      throw ApiException("cURL request failed: " +
                         std::string(curl_easy_strerror(res)));
    }

    if (status_code >= 400) {
      curl_easy_cleanup(curl);
      throw ApiException("HTTP error: " + std::to_string(status_code) +
                         ", Response: " + response);
    }

    curl_easy_cleanup(curl);
    return response;
  }

  static size_t write_callback(void *contents, size_t size, size_t nmemb,
                               void *userp) {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
  }

  std::string get_api_base_url() {
    try {
      // Try to access Config singleton
      auto &config_instance = config::Config::get_instance();
      bool sandbox = config_instance.sandbox_mode();
      return sandbox ? "https://api-public.sandbox.coinbase.com"
                     : "https://api.coinbase.com";
    } catch (const std::exception &ex) {
      LOG_ERROR("Failed to get sandbox mode from Config: {}", ex.what());
      return "https://api.coinbase.com"; // Default to production
    } catch (...) {
      LOG_ERROR("Failed to get sandbox mode from Config: unknown exception");
      return "https://api.coinbase.com"; // Default to production
    }
  }
};

CoinbaseClient::CoinbaseClient(const std::string &apiKey,
                               const std::string &apiSecret,
                               const std::string &apiPassphrase) {
  try {
    LOG_INFO("Creating CoinbaseClient Impl with API key: {}, Secret: {}, "
             "Passphrase: {}",
             apiKey.empty() ? "empty" : "***",
             apiSecret.empty() ? "empty" : "***",
             apiPassphrase.empty() ? "empty" : "***");
    pImpl = std::make_unique<Impl>(apiKey, apiSecret, apiPassphrase);
    LOG_INFO("CoinbaseClient Impl created successfully");
  } catch (const std::exception &ex) {
    LOG_ERROR("Failed to create CoinbaseClient implementation: {}", ex.what());
    LOG_ERROR("Exception type: {}", typeid(ex).name());
    pImpl = nullptr;
  } catch (...) {
    LOG_ERROR(
        "Failed to create CoinbaseClient implementation: unknown exception");
    pImpl = nullptr;
  }
}

CoinbaseClient::~CoinbaseClient() = default;

bool CoinbaseClient::test_connection() {
  if (!pImpl) {
    LOG_ERROR("CoinbaseClient not initialized");
    return false;
  }

  try {
    // Check if pImpl is valid before calling methods
    if (!pImpl) {
      LOG_ERROR("pImpl is null in test_connection");
      return false;
    }

    // Try to get product information (public endpoint)
    std::string url =
        pImpl->get_api_base_url() + "/api/v3/brokerage/products?limit=1";
    std::string response = pImpl->make_request(url);
    auto json_response = json::parse(response);

    if (json_response.contains("products")) {
      auto products = json_response.at("products");
      if (products.is_array() && !products.empty()) {
        LOG_INFO("Coinbase API connection test successful");
        return true;
      }
    }
    LOG_WARN("Coinbase API connection test returned empty response");
    return false;
  } catch (const std::exception &ex) {
    LOG_ERROR("Coinbase API connection test failed: {}", ex.what());
    return false;
  }
}

std::vector<models::SymbolInfo> CoinbaseClient::get_available_symbols() {
  try {
    std::string url = pImpl->get_api_base_url() + "/api/v3/brokerage/products";
    std::string response = pImpl->make_request(url);
    LOG_DEBUG("get_available_symbols response: {}",
              response); // Added debug log
    auto json_response = json::parse(response);

    // Check if response has "products" array (Coinbase Advanced Trade API
    // format)
    if (json_response.contains("products")) {
      auto products = json_response.at("products");
      std::vector<models::SymbolInfo> symbols;
      for (const auto &product : products) {
        std::string base_currency;
        if (product.contains("base_currency")) {
          base_currency = product.at("base_currency").get<std::string>();
        } else if (product.contains("base_currency_id")) {
          base_currency = product.at("base_currency_id").get<std::string>();
        }

        std::string quote_currency;
        if (product.contains("quote_currency")) {
          quote_currency = product.at("quote_currency").get<std::string>();
        } else if (product.contains("quote_currency_id")) {
          quote_currency = product.at("quote_currency_id").get<std::string>();
        }

        if (product.contains("product_id") && !base_currency.empty() &&
            !quote_currency.empty() && product.contains("display_name") &&
            product.contains("status")) {
          symbols.emplace_back(product.at("product_id").get<std::string>(),
                               base_currency, quote_currency,
                               product.at("display_name").get<std::string>(),
                               product.at("status").get<std::string>());
        } else {
          if (symbols.size() < 5) { // Log first 5 failures to avoid spam
            std::string missing;
            if (!product.contains("product_id"))
              missing += "product_id ";
            if (base_currency.empty())
              missing += "base_currency(_id) ";
            if (quote_currency.empty())
              missing += "quote_currency(_id) ";
            if (!product.contains("display_name"))
              missing += "display_name ";
            if (!product.contains("status"))
              missing += "status ";
            LOG_WARN("Skipping product due to missing fields: {}", missing);
          }
        }
      }
      LOG_INFO("Retrieved {} available symbols from Coinbase", symbols.size());
      return symbols;
    }

    // Fallback to old format (if API endpoint changes back)
    auto products = json::parse(response);
    std::vector<models::SymbolInfo> symbols;
    for (const auto &product : products) {
      if (product.contains("id") && product.contains("base_currency") &&
          product.contains("quote_currency") &&
          product.contains("display_name") && product.contains("status")) {
        symbols.emplace_back(product.at("id").get<std::string>(),
                             product.at("base_currency").get<std::string>(),
                             product.at("quote_currency").get<std::string>(),
                             product.at("display_name").get<std::string>(),
                             product.at("status").get<std::string>());
      }
    }
    LOG_INFO("Retrieved {} available symbols from Coinbase", symbols.size());
    return symbols;
  } catch (const std::exception &ex) {
    LOG_ERROR("Failed to get available symbols: {}", ex.what());
    return {};
  }
}

std::optional<models::SymbolInfo>
CoinbaseClient::get_symbol_info(const std::string &symbol) {
  try {
    std::string url =
        pImpl->get_api_base_url() + "/api/v3/brokerage/products/" + symbol;
    std::string response = pImpl->make_request(url);
    auto json_response = json::parse(response);

    // Check if response is in Coinbase Advanced Trade API format
    std::string base_currency;
    if (json_response.contains("base_currency")) {
      base_currency = json_response.at("base_currency").get<std::string>();
    } else if (json_response.contains("base_currency_id")) {
      base_currency = json_response.at("base_currency_id").get<std::string>();
    }

    std::string quote_currency;
    if (json_response.contains("quote_currency")) {
      quote_currency = json_response.at("quote_currency").get<std::string>();
    } else if (json_response.contains("quote_currency_id")) {
      quote_currency = json_response.at("quote_currency_id").get<std::string>();
    }

    if (json_response.contains("product_id") && !base_currency.empty() &&
        !quote_currency.empty() && json_response.contains("display_name") &&
        json_response.contains("status")) {
      LOG_DEBUG("Retrieved symbol info for {}", symbol);
      return models::SymbolInfo(
          json_response.at("product_id").get<std::string>(), base_currency,
          quote_currency, json_response.at("display_name").get<std::string>(),
          json_response.at("status").get<std::string>());
    }

    // Fallback to old format
    if (json_response.contains("id") &&
        json_response.contains("base_currency") &&
        json_response.contains("quote_currency") &&
        json_response.contains("display_name") &&
        json_response.contains("status")) {
      LOG_DEBUG("Retrieved symbol info for {}", symbol);
      return models::SymbolInfo(
          json_response.at("id").get<std::string>(),
          json_response.at("base_currency").get<std::string>(),
          json_response.at("quote_currency").get<std::string>(),
          json_response.at("display_name").get<std::string>(),
          json_response.at("status").get<std::string>());
    }

    LOG_WARN("No information found for symbol {}", symbol);
    return std::nullopt;
  } catch (const std::exception &ex) {
    LOG_ERROR("Failed to get symbol info for {}: {}", symbol, ex.what());
    return std::nullopt;
  }
}

std::optional<models::Decimal>
CoinbaseClient::get_current_price(const std::string &symbol) {
  try {
    auto start = system_clock::now() - std::chrono::hours(1);
    auto end = std::chrono::system_clock::now();
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
  } catch (const std::exception &ex) {
    LOG_ERROR("Failed to get current price for {}: {}", symbol, ex.what());
    return std::nullopt;
  }
}

// Helper to get granularity string
std::string get_granularity_string(int granularity) {
  switch (granularity) {
  case 60:
    return "ONE_MINUTE";
  case 300:
    return "FIVE_MINUTE";
  case 900:
    return "FIFTEEN_MINUTE";
  case 3600:
    return "ONE_HOUR";
  case 21600:
    return "SIX_HOUR";
  case 86400:
    return "ONE_DAY";
  default:
    return "ONE_HOUR"; // Default to 1 hour
  }
}

std::vector<models::CryptoPriceData> CoinbaseClient::get_historical_candles(
    const std::string &symbol, std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end, int granularity) {
  try {
    // Convert time_points to UNIX timestamp strings (seconds)
    auto start_ts =
        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                           start.time_since_epoch())
                           .count());
    auto end_ts = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(end.time_since_epoch())
            .count());

    std::string url = pImpl->get_api_base_url() +
                      "/api/v3/brokerage/products/" + symbol + "/candles?" +
                      "start=" + start_ts + "&end=" + end_ts +
                      "&granularity=" + get_granularity_string(granularity);

    std::string response = pImpl->make_request(url);
    auto json_response = json::parse(response);

    // Check if response has "candles" array (Coinbase Advanced Trade API
    // format)
    if (json_response.contains("candles")) {
      auto candles = json_response.at("candles");
      std::vector<models::CryptoPriceData> data_points;
      for (const auto &candle : candles) {
        if (candle.contains("start") && candle.contains("low") &&
            candle.contains("high") && candle.contains("open") &&
            candle.contains("close") && candle.contains("volume")) {
          auto timestamp = system_clock::time_point(std::chrono::seconds(
              std::stoll(candle.at("start").get<std::string>())));
          data_points.emplace_back(
              symbol, timestamp,
              models::Decimal(candle.at("open").get<std::string>()),
              models::Decimal(candle.at("high").get<std::string>()),
              models::Decimal(candle.at("low").get<std::string>()),
              models::Decimal(candle.at("close").get<std::string>()),
              models::Decimal(candle.at("volume").get<std::string>()));
        }
      }
      LOG_DEBUG("Retrieved {} candles for {}", data_points.size(), symbol);
      return data_points;
    }

    // Fallback to old format (array of arrays)
    auto candles = json::parse(response);
    std::vector<models::CryptoPriceData> data_points;
    for (const auto &candle : candles) {
      if (candle.size() >= 6) {
        auto timestamp = system_clock::time_point(
            std::chrono::seconds(candle[0].get<long long>()));
        data_points.emplace_back(symbol, timestamp,
                                 models::Decimal(candle[3].get<std::string>()),
                                 models::Decimal(candle[2].get<std::string>()),
                                 models::Decimal(candle[1].get<std::string>()),
                                 models::Decimal(candle[4].get<std::string>()),
                                 models::Decimal(candle[5].get<std::string>()));
      }
    }
    LOG_DEBUG("Retrieved {} candles for {}", data_points.size(), symbol);
    return data_points;
  } catch (const std::exception &ex) {
    LOG_ERROR("Failed to get historical candles for {}: {}", symbol, ex.what());
    return {};
  }
}

bool CoinbaseClient::is_symbol_available(const std::string &symbol) {
  auto symbol_info = get_symbol_info(symbol);
  if (symbol_info) {
    return symbol_info->status == "online";
  }
  return false;
}

bool CoinbaseClient::is_authenticated() const {
  if (!pImpl)
    return false;
  return !pImpl->apiKey_.empty() && !pImpl->apiSecret_.empty();
}

bool CoinbaseClient::sandbox_mode() const {
  return config::Config::get_instance().sandbox_mode();
}

RateLimiter::RateLimiter(int max_tokens, double refill_rate)
    : max_tokens_(max_tokens), refill_rate_(refill_rate),
      current_tokens_(max_tokens),
      last_refill_(std::chrono::system_clock::now()) {}

double RateLimiter::get_available_tokens() {
  auto now = system_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refill_)
          .count() /
      1000.0;

  double tokens_to_add = duration * refill_rate_;
  current_tokens_ = std::min(static_cast<double>(max_tokens_),
                             current_tokens_ + tokens_to_add);
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
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

RetryPolicy::RetryPolicy(int max_attempts, int base_delay_ms)
    : max_attempts_(max_attempts), base_delay_ms_(base_delay_ms) {}

bool RetryPolicy::should_retry(int attempt, const std::exception &ex) const {
  if (attempt >= max_attempts_ - 1) {
    return false;
  }

  const std::string &ex_msg = ex.what();
  if (ex_msg.find("400") != std::string::npos ||
      ex_msg.find("401") != std::string::npos ||
      ex_msg.find("403") != std::string::npos ||
      ex_msg.find("404") != std::string::npos) {
    return false;
  }

  return true;
}

std::chrono::system_clock::time_point
RetryPolicy::get_next_attempt_time(int attempt) const {
  int delay_ms = base_delay_ms_ * (1 << attempt);
  delay_ms = std::min(delay_ms, 10000);
  return system_clock::now() + std::chrono::milliseconds(delay_ms);
}

} // namespace api