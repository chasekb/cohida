#include "../../include/database/DatabaseManager.h"
#include "../../include/config/Config.h"
#include "../../include/utils/Logger.h"
#include "pqxx_strconv_specializations.h"
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sstream>

std::string format_time_point(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::chrono::system_clock::time_point string_to_time_point(const std::string& s) {
    std::tm tm = {};
    std::istringstream iss(s);
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(mktime(&tm));
}

namespace database {

std::unique_ptr<DatabaseManager> db_manager;

DatabaseManager::DatabaseManager(int granularity)
    : granularity_(granularity), schema_initialized_(false) {
    _initialize_connection_pool();
    _ensure_schema_exists();
}

DatabaseManager::~DatabaseManager() {
    close_connections();
}

void DatabaseManager::_initialize_connection_pool() {
    try {
        LOG_INFO("Initializing database connection pool");

        // Create initial connections
        for (int i = 0; i < MIN_POOL_SIZE; ++i) {
            auto conn = std::make_unique<pqxx::connection>(_get_connection_string());
            connection_pool_.push_back(std::move(conn));
        }

        LOG_INFO("Database connection pool initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize database connection pool: " + std::string(e.what()));
        throw DbException("Failed to initialize database connection pool: " + std::string(e.what()));
    }
}

std::string DatabaseManager::_get_connection_string() const {
    config::Config& config = config::Config::get_instance();
    
    std::string db_name = config.get_string("DB_NAME", "coinbase_data");
    if (granularity_ > 0) {
        db_name += "_" + std::to_string(granularity_);
    }
    
    std::ostringstream conn_str;
    conn_str << "host=" << config.get_string("DB_HOST", "localhost")
             << " port=" << config.get_int("DB_PORT", 5432)
             << " dbname=" << db_name
             << " user=" << config.get_string("DB_USER", "postgres")
             << " password=" << config.get_string("DB_PASSWORD", "password");
    
    return conn_str.str();
}

std::string DatabaseManager::_get_table_name() const {
    config::Config& config = config::Config::get_instance();
    std::string table_name = config.get_string("DB_TABLE", "crypto_prices");
    
    if (granularity_ > 0) {
        table_name += "_" + std::to_string(granularity_);
    }
    
    return table_name;
}

std::string DatabaseManager::_get_full_table_name() const {
    config::Config& config = config::Config::get_instance();
    std::string schema = config.get_string("DB_SCHEMA", "public");
    return schema + "." + _get_table_name();
}

std::unique_ptr<pqxx::connection> DatabaseManager::_get_connection() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (!connection_pool_.empty()) {
        auto conn = std::move(connection_pool_.back());
        connection_pool_.pop_back();
        
        if (conn->is_open()) {
            return conn;
        }
    }
    
    // Create new connection if pool is empty and we haven't reached max size
    if (connection_pool_.size() < static_cast<size_t>(MAX_POOL_SIZE)) {
        try {
            return std::make_unique<pqxx::connection>(_get_connection_string());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create new database connection: " + std::string(e.what()));
            throw DbException("Failed to create new database connection: " + std::string(e.what()));
        }
    }
    
    throw DbException("Database connection pool exhausted");
}

void DatabaseManager::_return_connection(std::unique_ptr<pqxx::connection> conn) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (conn && conn->is_open()) {
        connection_pool_.push_back(std::move(conn));
    }
}

void DatabaseManager::_ensure_schema_exists() {
    if (schema_initialized_) {
        return;
    }
    
    try {
        auto conn = _get_connection();
        pqxx::work txn(*conn);
        
        config::Config& config = config::Config::get_instance();
        std::string schema = config.get_string("DB_SCHEMA", "public");
        
        // Create schema if it doesn't exist
        txn.exec("CREATE SCHEMA IF NOT EXISTS " + txn.quote_name(schema) + ";");
        
        // Create main table
        std::string full_table_name = _get_full_table_name();
        std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS )" + txn.quote_name(full_table_name) + R"( (
                id SERIAL PRIMARY KEY,
                symbol TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                open_price NUMERIC(18, 8) NOT NULL,
                high_price NUMERIC(18, 8) NOT NULL,
                low_price NUMERIC(18, 8) NOT NULL,
                close_price NUMERIC(18, 8) NOT NULL,
                volume NUMERIC(20, 8) NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(symbol, timestamp)
            );
        )";
        
        txn.exec(create_table_sql);
        
        // Create indexes
        std::string symbol_timestamp_index = "idx_" + _get_table_name() + "_symbol_timestamp";
        txn.exec("CREATE INDEX IF NOT EXISTS " + txn.quote_name(symbol_timestamp_index) + 
                " ON " + txn.quote_name(full_table_name) + " (symbol, timestamp);");
        
        std::string timestamp_index = "idx_" + _get_table_name() + "_timestamp";
        txn.exec("CREATE INDEX IF NOT EXISTS " + txn.quote_name(timestamp_index) + 
                " ON " + txn.quote_name(full_table_name) + " (timestamp);");
        
        txn.commit();
        _return_connection(std::move(conn));
        
        schema_initialized_ = true;
        LOG_INFO("Database schema verified and created if needed");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to ensure schema exists: " + std::string(e.what()));
        throw DbException("Failed to ensure schema exists: " + std::string(e.what()));
    }
}

bool DatabaseManager::test_connection() {
    try {
        auto conn = _get_connection();
        pqxx::work txn(*conn);
        auto result = txn.exec("SELECT 1");
        txn.commit();
        _return_connection(std::move(conn));
        
        LOG_INFO("Database connection test successful");
        return !result.empty() && result[0][0].as<int>() == 1;
    } catch (const std::exception& e) {
        LOG_ERROR("Database connection test failed: " + std::string(e.what()));
        return false;
    }
}

void DatabaseManager::close_connections() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    connection_pool_.clear();
    LOG_INFO("All database connections closed");
}

int DatabaseManager::write_data(const std::vector<models::CryptoPriceData>& data_points) {
    if (data_points.empty()) {
        LOG_WARN("No data points provided for writing");
        return 0;
    }
    
    int written_count = 0;
    
    try {
        auto conn = _get_connection();
        pqxx::work txn(*conn);
        
        std::string full_table_name = _get_full_table_name();
        std::string insert_sql = R"(
            INSERT INTO )" + txn.quote_name(full_table_name) + R"(
            (symbol, timestamp, open_price, high_price, low_price, close_price, volume, updated_at)
            VALUES ($1, $2, $3, $4, $5, $6, $7, CURRENT_TIMESTAMP)
            ON CONFLICT (symbol, timestamp) 
            DO UPDATE SET 
                open_price = EXCLUDED.open_price,
                high_price = EXCLUDED.high_price,
                low_price = EXCLUDED.low_price,
                close_price = EXCLUDED.close_price,
                volume = EXCLUDED.volume,
                updated_at = EXCLUDED.updated_at
        )";
        
        for (const auto& data_point : data_points) {
            try {
                std::ostringstream oss;
                oss << data_point.open_price;
                std::string open_str = oss.str();
                oss.str("");
                oss << data_point.high_price;
                std::string high_str = oss.str();
                oss.str("");
                oss << data_point.low_price;
                std::string low_str = oss.str();
                oss.str("");
                oss << data_point.close_price;
                std::string close_str = oss.str();
                oss.str("");
                oss << data_point.volume;
                std::string volume_str = oss.str();

                txn.exec(pqxx::zview(insert_sql), 
                    pqxx::params{data_point.symbol,
                    format_time_point(data_point.timestamp),
                    open_str,
                    high_str,
                    low_str,
                    close_str,
                    volume_str}
                );
                written_count++;
                
                LOG_DEBUG("Written data point for " + data_point.symbol + 
                                    " at " + format_time_point(data_point.timestamp));
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to write data point for " + data_point.symbol + 
                                     ": " + std::string(e.what()));
                continue;
            }
        }
        
        txn.commit();
        _return_connection(std::move(conn));
        
        LOG_INFO("Successfully wrote " + std::to_string(written_count) + 
                          " data points to database");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to write data to database: " + std::string(e.what()));
        throw DbException("Failed to write data to database: " + std::string(e.what()));
    }
    
    return written_count;
}

std::vector<models::CryptoPriceData> DatabaseManager::read_data(const std::string& symbol, 
                                                       const std::chrono::system_clock::time_point& start_date,
                                                       const std::chrono::system_clock::time_point& end_date) {
    std::vector<models::CryptoPriceData> data_points;
    
    try {
        auto conn = _get_connection();
        pqxx::work txn(*conn);
        
        std::string full_table_name = _get_full_table_name();
        std::string select_sql = R"(
            SELECT symbol, timestamp, open_price, high_price, low_price, close_price, volume
            FROM )" + txn.quote_name(full_table_name) + R"(
            WHERE symbol = $1 AND timestamp BETWEEN $2 AND $3
            ORDER BY timestamp ASC
        )";
        
        auto result = txn.exec(pqxx::zview(select_sql), pqxx::params{symbol, format_time_point(start_date), format_time_point(end_date)});
        
        for (const auto& row : result) {
            try {
                models::CryptoPriceData data_point(
                    row["symbol"].as<std::string>(),
                    string_to_time_point(row["timestamp"].as<std::string>()),
                    row["open_price"].as<double>(),
                    row["high_price"].as<double>(),
                    row["low_price"].as<double>(),
                    row["close_price"].as<double>(),
                    row["volume"].as<double>()
                );
                
                data_points.push_back(std::move(data_point));
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to parse data row: " + std::string(e.what()));
                continue;
            }
        }
        
        txn.commit();
        _return_connection(std::move(conn));
        
        LOG_INFO("Retrieved " + std::to_string(data_points.size()) + 
                          " data points for " + symbol);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read data from database: " + std::string(e.what()));
        throw DbException("Failed to read data from database: " + std::string(e.what()));
    }
    
    return data_points;
}

int DatabaseManager::get_data_count(const std::string& symbol) {
    try {
        auto conn = _get_connection();
        pqxx::work txn(*conn);
        
        std::string full_table_name = _get_full_table_name();
        std::string count_sql = "SELECT COUNT(*) FROM " + txn.quote_name(full_table_name) + 
                               " WHERE symbol = $1";
        
        auto result = txn.exec(pqxx::zview(count_sql), pqxx::params{symbol});
        txn.commit();
        _return_connection(std::move(conn));
        
        int count = result[0][0].as<int>();
        LOG_DEBUG("Data count for " + symbol + ": " + std::to_string(count));
        
        return count;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get data count for " + symbol + ": " + std::string(e.what()));
        return 0;
    }
}

std::optional<std::chrono::system_clock::time_point> DatabaseManager::get_latest_timestamp(const std::string& symbol) {
    try {
        auto conn = _get_connection();
        pqxx::work txn(*conn);
        
        std::string full_table_name = _get_full_table_name();
        std::string latest_sql = "SELECT MAX(timestamp) FROM " + txn.quote_name(full_table_name) + 
                               " WHERE symbol = $1";
        
        auto result = txn.exec(pqxx::zview(latest_sql), pqxx::params{symbol});
        txn.commit();
        _return_connection(std::move(conn));
        
        if (result[0][0].is_null()) {
            return std::nullopt;
        }
        
        auto latest = string_to_time_point(result[0][0].as<std::string>());
        LOG_DEBUG("Latest timestamp for " + symbol + ": " + 
                           format_time_point(latest));
        
        return latest;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get latest timestamp for " + symbol + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

} // namespace database