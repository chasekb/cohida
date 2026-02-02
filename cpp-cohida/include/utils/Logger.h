#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <string>
#include <memory>

namespace utils {

class Logger {
public:
    static void initialize(const std::string& log_level = "info",
                          const std::string& log_file = "logs/cohida.log");
    
    static std::shared_ptr<spdlog::logger>& get_logger() {
        return logger_;
    }
    
    static void set_level(const std::string& level);
    static void set_pattern(const std::string& pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    
private:
    static std::shared_ptr<spdlog::logger> logger_;
};

// Helper macros for convenient logging
#define LOG_TRACE(...) utils::Logger::get_logger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) utils::Logger::get_logger()->debug(__VA_ARGS__)
#define LOG_INFO(...) utils::Logger::get_logger()->info(__VA_ARGS__)
#define LOG_WARN(...) utils::Logger::get_logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) utils::Logger::get_logger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) utils::Logger::get_logger()->critical(__VA_ARGS__)

} // namespace utils