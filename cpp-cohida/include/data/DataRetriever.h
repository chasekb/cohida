#ifndef DATA_RETRIEVER_H
#define DATA_RETRIEVER_H

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include "utils/Logger.h"
#include "api/CoinbaseClient.h"
#include "models/DataPoint.h"

namespace data {

using namespace std::chrono;
using json = nlohmann::json;

struct DataRetrievalRequest {
    std::string symbol;
    system_clock::time_point start_date;
    system_clock::time_point end_date;
    int granularity;
    bool skip_validation;

    DataRetrievalRequest(std::string symbol,
                       system_clock::time_point start_date,
                       system_clock::time_point end_date,
                       int granularity,
                       bool skip_validation = false)
        : symbol(std::move(symbol)),
          start_date(start_date),
          end_date(end_date),
          granularity(granularity),
          skip_validation(skip_validation) {}
};

struct DataRetrievalResult {
    std::string symbol;
    bool success;
    std::vector<models::CryptoPriceData> data_points;
    std::string error_message;

    DataRetrievalResult(std::string symbol, bool success,
                       const std::vector<models::CryptoPriceData>& data_points = {},
                       const std::string& error_message = "")
        : symbol(std::move(symbol)), success(success), data_points(data_points),
          error_message(error_message) {}
};

class DataRetriever {
public:
    DataRetriever();
    ~DataRetriever();

    /**
     * @brief Retrieve historical data for a specific symbol and time range
     * @param symbol The cryptocurrency symbol to retrieve data for
     * @param start_date The start date of the historical data
     * @param end_date The end date of the historical data
     * @param granularity The time interval between data points (in seconds)
     * @return DataRetrievalResult containing the retrieved data
     */
    DataRetrievalResult retrieve_historical_data(const std::string& symbol,
                                               system_clock::time_point start_date,
                                               system_clock::time_point end_date,
                                               int granularity);

    /**
     * @brief Retrieve all available historical data for a specific symbol
     * @param symbol The cryptocurrency symbol to retrieve data for
     * @param granularity The time interval between data points (in seconds)
     * @param max_records Optional maximum number of records to retrieve (unlimited if not specified)
     * @return DataRetrievalResult containing all available data
     */
    DataRetrievalResult retrieve_all_historical_data(const std::string& symbol,
                                                   int granularity,
                                                   std::optional<int> max_records = std::nullopt);

    /**
     * @brief Validate a symbol against Coinbase API
     * @param symbol The symbol to validate
     * @return True if the symbol is valid and available
     */
    bool validate_symbol(const std::string& symbol);

    /**
     * @brief Check if data retrieval is currently in progress
     * @return True if data retrieval is in progress
     */
    bool is_retrieving() const { return is_retrieving_; }

private:
    std::unique_ptr<api::CoinbaseClient> coinbase_client_;
    bool is_retrieving_;

    DataRetrievalResult retrieve_historical_data(const DataRetrievalRequest& request);
    std::vector<models::CryptoPriceData> _fetch_data_from_api(const DataRetrievalRequest& request);
    std::vector<models::CryptoPriceData> _transform_api_data(const std::vector<json>& raw_data,
                                                          const std::string& symbol);
    system_clock::time_point _find_earliest_available_data(const std::string& symbol, int granularity,
                                                        system_clock::time_point max_test_date);

    void _log_retrieval_progress(int current, int total);
};

} // namespace data

#endif // DATA_RETRIEVER_H