#include <iostream>
#include "config/Config.h"
#include "utils/Logger.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    try {
        // Initialize logger
        utils::Logger::initialize("debug", "logs/test_config.log");
        
        // Load configuration from .env file
        config::Config& config = config::Config::get_instance();
        config.load(".env");
        
        LOG_INFO("Configuration loaded successfully");
        
        // Log all configuration keys
        LOG_DEBUG("All configuration keys:");
        for (const auto& [key, value] : config.get_all()) {
            LOG_DEBUG("  {} = {}", key, value);
        }
        
        // Test configuration retrieval
        std::string db_host = config.get_string("DB_HOST");
        int db_port = config.get_int("DB_PORT");
        std::string db_name = config.get_string("DB_NAME");
        
        LOG_INFO("Database configuration:");
        LOG_INFO("  Host: {}", db_host);
        LOG_INFO("  Port: {}", db_port);
        LOG_INFO("  Name: {}", db_name);
        
        // Test optional configuration
        auto api_key = config.get_optional_string("COINBASE_API_KEY");
        if (api_key) {
            LOG_INFO("Coinbase API key: {}", api_key.value());
        } else {
            LOG_WARN("Coinbase API key not configured");
        }
        
        LOG_INFO("Test completed successfully");
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}