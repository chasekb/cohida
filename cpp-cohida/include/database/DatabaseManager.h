#pragma once

#include <vector>
#include <optional>
#include <string>
#include <memory>
#include <pqxx/pqxx>
#include "DbException.h"
#include "../models/DataPoint.h"
#include "../config/Config.h"

namespace database {

class DatabaseManager {
public:
    DatabaseManager(int granularity = -1);
    ~DatabaseManager();
    
    // Prevent copy and assignment
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    // Connection management
    bool test_connection();
    void close_connections();
    
    // Data operations
    int write_data(const std::vector<models::CryptoPriceData>& data_points);
    std::vector<models::CryptoPriceData> read_data(const std::string& symbol, 
                                            const std::chrono::system_clock::time_point& start_date,
                                            const std::chrono::system_clock::time_point& end_date);
    int get_data_count(const std::string& symbol);
    std::optional<std::chrono::system_clock::time_point> get_latest_timestamp(const std::string& symbol);
    
private:
    void _initialize_connection_pool();
    void _ensure_schema_exists();
    std::unique_ptr<pqxx::connection> _get_connection();
    void _return_connection(std::unique_ptr<pqxx::connection> conn);
    
    std::string _get_table_name() const;
    std::string _get_schema_name() const;
    std::string _get_full_table_name() const;
    std::string _get_connection_string() const;
    
    int granularity_;
    std::vector<std::unique_ptr<pqxx::connection>> connection_pool_;
    std::mutex pool_mutex_;
    bool schema_initialized_;
    
    const int MIN_POOL_SIZE = 1;
    const int MAX_POOL_SIZE = 10;
};

// Global database manager instance (no granularity for backward compatibility)
extern std::unique_ptr<DatabaseManager> db_manager;

} // namespace database