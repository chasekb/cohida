#ifndef DATA_POINT_H
#define DATA_POINT_H

#include <string>
#include <chrono>
#include <stdexcept>
#include <optional>
#include <ostream>
#include <nlohmann/json.hpp>
#include "utils/Logger.h"
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/iostream.hpp>
#include <algorithm>
#include <fmt/format.h>

namespace models {

using namespace std::chrono;
using json = nlohmann::json;
using Decimal = boost::decimal::decimal128_t;

class CryptoPriceData {
public:
    std::string symbol;
    system_clock::time_point timestamp;
    Decimal open_price;
    Decimal high_price;
    Decimal low_price;
    Decimal close_price;
    Decimal volume;

    CryptoPriceData(std::string symbol,
                    system_clock::time_point timestamp,
                    Decimal open_price,
                    Decimal high_price,
                    Decimal low_price,
                    Decimal close_price,
                    Decimal volume);

    json to_json() const;
    static CryptoPriceData from_json(const json& j);
    bool isValid() const;
};

class SymbolInfo {
public:
    std::string symbol;
    std::string base_currency;
    std::string quote_currency;
    std::string display_name;
    std::string status;

    SymbolInfo(std::string symbol,
               std::string base_currency,
               std::string quote_currency,
               std::string display_name,
               std::string status);

    json to_json() const;
    static SymbolInfo from_json(const json& j);
    bool isValid() const;
};

class DataRetrievalRequest {
public:
    std::string symbol;
    system_clock::time_point start_date;
    system_clock::time_point end_date;
    int granularity;
    bool skip_validation;

    DataRetrievalRequest(std::string symbol,
                        system_clock::time_point start_date,
                        system_clock::time_point end_date,
                        int granularity = 3600,
                        bool skip_validation = false);

    bool isValid() const;
};

class DataRetrievalResult {
public:
    std::string symbol;
    bool success;
    std::vector<CryptoPriceData> data_points;
    std::optional<std::string> error_message;
    system_clock::time_point retrieved_at;

    DataRetrievalResult(std::string symbol,
                       bool success,
                       std::vector<CryptoPriceData> data_points,
                       std::optional<std::string> error_message = std::nullopt);

    int data_count() const;
    bool is_empty() const;
};

class SymbolValidator {
public:
    static bool is_valid_symbol(const std::string& symbol);
    static std::string normalize_symbol(const std::string& symbol);
};

} // namespace models

// ostream operator for Decimal type
inline std::ostream& operator<<(std::ostream& os, const models::Decimal& d) {
    os << boost::decimal::to_float<models::Decimal, double>(d);
    return os;
}

// fmt formatter for Decimal type (must be in fmt namespace, outside models)
template<>
struct fmt::formatter<models::Decimal> : fmt::formatter<double> {
    template<typename FormatContext>
    auto format(models::Decimal d, FormatContext& ctx) const {
        return fmt::formatter<double>::format(boost::decimal::to_float<models::Decimal, double>(d), ctx);
    }
};

#endif // DATA_POINT_H
