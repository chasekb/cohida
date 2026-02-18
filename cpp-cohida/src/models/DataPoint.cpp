#include "models/DataPoint.h"

namespace models {

CryptoPriceData::CryptoPriceData(std::string symbol,
                                 system_clock::time_point timestamp,
                                 Decimal open_price, Decimal high_price,
                                 Decimal low_price, Decimal close_price,
                                 Decimal volume)
    : symbol(std::move(symbol)), timestamp(timestamp), open_price(open_price),
      high_price(high_price), low_price(low_price), close_price(close_price),
      volume(volume) {
  if (!isValid()) {
    throw std::invalid_argument("Invalid CryptoPriceData");
  }
}

json CryptoPriceData::to_json() const {
  auto timestamp_ms =
      duration_cast<milliseconds>(timestamp.time_since_epoch()).count();
  std::stringstream oss;
  oss << open_price;

  return {{"symbol", symbol},
          {"timestamp", timestamp_ms},
          {"open_price", oss.str()},
          {"high_price", (std::ostringstream() << high_price).str()},
          {"low_price", (std::ostringstream() << low_price).str()},
          {"close_price", (std::ostringstream() << close_price).str()},
          {"volume", (std::ostringstream() << volume).str()}};
}

CryptoPriceData CryptoPriceData::from_json(const json &j) {
  system_clock::time_point timestamp;
  if (j.contains("timestamp")) {
    if (j["timestamp"].is_number()) {
      auto ms = j["timestamp"].get<long long>();
      timestamp = system_clock::time_point(milliseconds(ms));
    }
  }

  return CryptoPriceData(j.at("symbol").get<std::string>(), timestamp,
                         Decimal(j.at("open_price").get<std::string>()),
                         Decimal(j.at("high_price").get<std::string>()),
                         Decimal(j.at("low_price").get<std::string>()),
                         Decimal(j.at("close_price").get<std::string>()),
                         Decimal(j.at("volume").get<std::string>()));
}

bool CryptoPriceData::isValid() const {
  if (symbol.empty()) {
    LOG_WARN("Symbol cannot be empty");
    return false;
  }

  const Decimal zero(0);
  if (open_price <= zero || high_price <= zero || low_price <= zero ||
      close_price <= zero) {
    LOG_WARN("All prices must be positive");
    return false;
  }

  if (volume < zero) {
    LOG_WARN("Volume cannot be negative");
    return false;
  }

  if (high_price < std::max(open_price, close_price)) {
    LOG_WARN("High price is lower than open/close prices for " + symbol);
  }

  if (low_price > std::min(open_price, close_price)) {
    LOG_WARN("Low price is higher than open/close prices for " + symbol);
  }

  return true;
}

SymbolInfo::SymbolInfo(std::string symbol, std::string base_currency,
                       std::string quote_currency, std::string display_name,
                       std::string status)
    : symbol(std::move(symbol)), base_currency(std::move(base_currency)),
      quote_currency(std::move(quote_currency)),
      display_name(std::move(display_name)), status(std::move(status)) {
  if (!isValid()) {
    throw std::invalid_argument("Invalid SymbolInfo");
  }
}

json SymbolInfo::to_json() const {
  return {{"symbol", symbol},
          {"base_currency", base_currency},
          {"quote_currency", quote_currency},
          {"display_name", display_name},
          {"status", status}};
}

SymbolInfo SymbolInfo::from_json(const json &j) {
  return SymbolInfo(j.at("symbol").get<std::string>(),
                    j.at("base_currency").get<std::string>(),
                    j.at("quote_currency").get<std::string>(),
                    j.at("display_name").get<std::string>(),
                    j.at("status").get<std::string>());
}

bool SymbolInfo::isValid() const {
  if (symbol.empty()) {
    LOG_WARN("Symbol cannot be empty");
    return false;
  }

  if (base_currency.empty() || quote_currency.empty()) {
    LOG_WARN("Base and quote currencies must be specified");
    return false;
  }

  if (status != "online" && status != "offline" && status != "delisted") {
    LOG_WARN("Unknown status '" + status + "' for symbol " + symbol);
  }

  return true;
}

DataRetrievalRequest::DataRetrievalRequest(std::string symbol,
                                           system_clock::time_point start_date,
                                           system_clock::time_point end_date,
                                           int granularity,
                                           bool skip_validation)
    : symbol(std::move(symbol)), start_date(start_date), end_date(end_date),
      granularity(granularity), skip_validation(skip_validation) {
  if (!isValid()) {
    throw std::invalid_argument("Invalid DataRetrievalRequest");
  }
}

bool DataRetrievalRequest::isValid() const {
  if (symbol.empty()) {
    LOG_WARN("Symbol cannot be empty");
    return false;
  }

  if (start_date >= end_date) {
    LOG_WARN("Start date must be before end date");
    return false;
  }

  std::vector<int> valid_granularities = {60, 300, 900, 3600, 21600, 86400};
  auto it = std::find(valid_granularities.begin(), valid_granularities.end(),
                      granularity);
  if (it == valid_granularities.end()) {
    LOG_WARN("Granularity must be one of: 60, 300, 900, 3600, 21600, 86400");
    return false;
  }

  if (!skip_validation) {
    const int MAX_DATA_POINTS = 299;
    auto duration = duration_cast<seconds>(end_date - start_date).count();
    auto max_duration = static_cast<long long>(granularity) * MAX_DATA_POINTS;

    if (duration > max_duration) {
      LOG_WARN("Date range too large for granularity " +
               std::to_string(granularity) +
               ". Max duration: " + std::to_string(max_duration) + " seconds");
      return false;
    }
  }

  return true;
}

DataRetrievalResult::DataRetrievalResult(
    std::string symbol, bool success, std::vector<CryptoPriceData> data_points,
    std::optional<std::string> error_message)
    : symbol(std::move(symbol)), success(success),
      data_points(std::move(data_points)), error_message(error_message),
      retrieved_at(system_clock::now()) {}

int DataRetrievalResult::data_count() const { return data_points.size(); }

bool DataRetrievalResult::is_empty() const { return data_points.empty(); }

bool SymbolValidator::is_valid_symbol(const std::string &symbol) {
  if (symbol.empty()) {
    return false;
  }

  size_t dash_pos = symbol.find('-');
  if (dash_pos == std::string::npos) {
    return false;
  }

  std::string base = symbol.substr(0, dash_pos);
  std::string quote = symbol.substr(dash_pos + 1);

  if (base.empty() || quote.empty()) {
    return false;
  }

  if (base.length() < 3 || base.length() > 6 || quote.length() != 3) {
    return false;
  }

  for (char c : base) {
    if (!std::isalnum(static_cast<unsigned char>(c))) {
      return false;
    }
  }

  for (char c : quote) {
    if (!std::isalnum(static_cast<unsigned char>(c))) {
      return false;
    }
  }

  return true;
}

std::string SymbolValidator::normalize_symbol(const std::string &symbol) {
  if (symbol.empty()) {
    throw std::invalid_argument("Symbol cannot be empty");
  }

  std::string normalized = symbol;
  std::transform(
      normalized.begin(), normalized.end(), normalized.begin(),
      [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

  normalized.erase(
      normalized.begin(),
      std::find_if(normalized.begin(), normalized.end(),
                   [](unsigned char c) { return !std::isspace(c); }));
  normalized.erase(
      std::find_if(normalized.rbegin(), normalized.rend(),
                   [](unsigned char c) { return !std::isspace(c); })
          .base(),
      normalized.end());

  if (!is_valid_symbol(normalized)) {
    LOG_WARN("Symbol '" + symbol + "' may not be supported");
  }

  return normalized;
}

} // namespace models