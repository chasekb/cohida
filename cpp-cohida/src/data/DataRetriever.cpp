#include "data/DataRetriever.h"
#include <iomanip>

std::string format_time_point(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

namespace data {

DataRetriever::DataRetriever() : is_retrieving_(false) {
    coinbase_client_ = std::make_unique<api::CoinbaseClient>();
    
    if (!coinbase_client_->is_authenticated()) {
        LOG_WARN("Coinbase client not authenticated");
    }
    
    LOG_INFO("DataRetriever initialized successfully");
}

DataRetriever::~DataRetriever() {
    LOG_INFO("DataRetriever shutting down");
}

DataRetrievalResult DataRetriever::retrieve_historical_data(const std::string& symbol,
                                                           system_clock::time_point start_date,
                                                           system_clock::time_point end_date,
                                                           int granularity) {
    DataRetrievalRequest request(symbol, start_date, end_date, granularity);
    return retrieve_historical_data(request);
}

DataRetrievalResult DataRetriever::retrieve_historical_data(const DataRetrievalRequest& request) {
    is_retrieving_ = true;
    LOG_INFO("Starting historical data retrieval for " + request.symbol);

    try {
        auto data_points = _fetch_data_from_api(request);
        
        if (data_points.empty()) {
            LOG_WARN("No data points retrieved for symbol: " + request.symbol);
            is_retrieving_ = false;
            return DataRetrievalResult(request.symbol, true, {}, "No data points available");
        }
        
        LOG_INFO("Successfully retrieved " + std::to_string(data_points.size()) + 
                  " data points for symbol: " + request.symbol);
        
        is_retrieving_ = false;
        return DataRetrievalResult(request.symbol, true, data_points);
    } catch (const std::exception& ex) {
        LOG_ERROR("Error retrieving historical data: " + std::string(ex.what()));
        is_retrieving_ = false;
        return DataRetrievalResult(request.symbol, false, {}, ex.what());
    }
}

std::vector<models::CryptoPriceData> DataRetriever::_fetch_data_from_api(const DataRetrievalRequest& request) {
    try {
        auto candles = coinbase_client_->get_historical_candles(
            request.symbol,
            request.start_date,
            request.end_date,
            request.granularity
        );
        
        LOG_DEBUG("API returned " + std::to_string(candles.size()) + " candles for symbol: " + request.symbol);
        return candles;
    } catch (const api::ApiException& ex) {
        LOG_ERROR("API Error: " + std::string(ex.what()));
        throw;
    } catch (const std::exception& ex) {
        LOG_ERROR("Error fetching data from API: " + std::string(ex.what()));
        throw;
    }
}

std::vector<models::CryptoPriceData> DataRetriever::_transform_api_data(const std::vector<json>& raw_data,
                                                                      const std::string& symbol) {
    std::vector<models::CryptoPriceData> data_points;
    data_points.reserve(raw_data.size());

    for (const auto& candle : raw_data) {
        try {
            if (!candle.is_array() || candle.size() < 6) {
                LOG_WARN("Invalid candle format: " + candle.dump());
                continue;
            }

            auto timestamp = system_clock::time_point(seconds(static_cast<long long>(candle[0])));
            
            models::CryptoPriceData data_point(
                symbol,
                timestamp,
                models::Decimal(candle[3].get<std::string>()),
                models::Decimal(candle[2].get<std::string>()),
                models::Decimal(candle[1].get<std::string>()),
                models::Decimal(candle[4].get<std::string>()),
                models::Decimal(candle[5].get<std::string>())
            );

            data_points.push_back(std::move(data_point));
        } catch (const std::exception& ex) {
            LOG_WARN("Failed to transform candle data: " + std::string(ex.what()));
            continue;
        }
    }

    LOG_DEBUG("Transformed " + std::to_string(data_points.size()) + " data points from API response");
    return data_points;
}

system_clock::time_point DataRetriever::_find_earliest_available_data(const std::string& symbol, int granularity,
                                                                system_clock::time_point max_test_date) {
    LOG_INFO("Finding earliest available data for " + symbol);
    
    const int MAX_YEARS_BACK = 10;
    const int TEST_WINDOW_DAYS = 7;
    
    auto current = system_clock::now();
    for (int years_back = 1; years_back <= MAX_YEARS_BACK; ++years_back) {
        auto test_start = current - years(years_back);
        auto test_end = test_start + days(TEST_WINDOW_DAYS);
        
        if (test_end > max_test_date) {
            LOG_DEBUG("Skipping test for " + std::to_string(years_back) + " years back - exceeds max date");
            continue;
        }
        
        LOG_DEBUG("Testing data availability from " + 
                  format_time_point(test_start) + " to " + 
                  format_time_point(test_end));
        
        try {
            DataRetrievalRequest request(symbol, test_start, test_end, granularity, true);
            auto test_result = retrieve_historical_data(request);
            
            if (test_result.success && !test_result.data_points.empty()) {
                auto min_timestamp = std::min_element(
                    test_result.data_points.begin(),
                    test_result.data_points.end(),
                    [](const models::CryptoPriceData& a, const models::CryptoPriceData& b) {
                        return a.timestamp < b.timestamp;
                    })->timestamp;
                
                LOG_DEBUG("Found data from " + std::to_string(years_back) + 
                          " years back: " + format_time_point(min_timestamp));
                return min_timestamp;
            }
        } catch (const std::exception& ex) {
            LOG_DEBUG("No data found from " + std::to_string(years_back) + " years back");
        }
    }
    
    LOG_WARN("Failed to find earliest available data, using default 1 year back");
    return current - years(1);
}

DataRetrievalResult DataRetriever::retrieve_all_historical_data(const std::string& symbol, int granularity,
                                                               std::optional<int> max_records) {
    LOG_INFO("Starting complete historical data retrieval for " + symbol);
    
    try {
        auto end_date = system_clock::now();
        auto start_date = _find_earliest_available_data(symbol, granularity, end_date - years(10));
        
        LOG_INFO("Auto-detected earliest data for " + symbol + ": " + 
                  format_time_point(start_date));
        
        std::vector<models::CryptoPriceData> all_data_points;
        
        const int MAX_CANDLES_PER_REQUEST = 200;
        const int REQUEST_INTERVAL_SECONDS = granularity * MAX_CANDLES_PER_REQUEST;
        
        auto chunk_start = start_date;
        int chunk_count = 0;
        
        while (chunk_start < end_date) {
            auto chunk_end = chunk_start + seconds(REQUEST_INTERVAL_SECONDS);
            if (chunk_end > end_date) {
                chunk_end = end_date;
            }
            
            LOG_INFO("Processing chunk " + std::to_string(chunk_count + 1) + ": " +
                      format_time_point(chunk_start) + " to " + format_time_point(chunk_end));
            
            try {
                DataRetrievalRequest request(symbol, chunk_start, chunk_end, granularity, true);
                auto chunk_result = retrieve_historical_data(request);
                
                if (chunk_result.success && !chunk_result.data_points.empty()) {
                    all_data_points.insert(
                        all_data_points.end(),
                        std::make_move_iterator(chunk_result.data_points.begin()),
                        std::make_move_iterator(chunk_result.data_points.end())
                    );
                    
                    LOG_DEBUG("Chunk " + std::to_string(chunk_count + 1) + 
                              " retrieved " + std::to_string(chunk_result.data_points.size()) + " data points");
                } else {
                    LOG_WARN("Chunk " + std::to_string(chunk_count + 1) + " failed: " + chunk_result.error_message);
                }
            } catch (const std::exception& ex) {
                LOG_WARN("Chunk " + std::to_string(chunk_count + 1) + " failed: " + ex.what());
            }
            
            chunk_start = chunk_end + seconds(1);
            chunk_count++;
            
            if (max_records && static_cast<int>(all_data_points.size()) >= *max_records) {
                LOG_INFO("Reached maximum record limit: " + std::to_string(*max_records));
                all_data_points.erase(all_data_points.begin() + *max_records, all_data_points.end());
                break;
            }
        }
        
        LOG_INFO("Complete historical data retrieval finished");
        
        return DataRetrievalResult(symbol, true, all_data_points);
    } catch (const std::exception& ex) {
        LOG_ERROR("Error in complete historical data retrieval: " + std::string(ex.what()));
        return DataRetrievalResult(symbol, false, {}, ex.what());
    }
}

bool DataRetriever::validate_symbol(const std::string& symbol) {
    try {
        auto symbol_info = coinbase_client_->get_symbol_info(symbol);
        return symbol_info.has_value();
    } catch (const std::exception& ex) {
        LOG_ERROR("Error validating symbol: " + std::string(ex.what()));
        return false;
    }
}

void DataRetriever::_log_retrieval_progress(int current, int total) {
    if (total > 0) {
        double progress = static_cast<double>(current) / total * 100;
        LOG_INFO("Data retrieval progress: " + std::to_string(static_cast<int>(progress)) + "%");
    }
}

} // namespace data