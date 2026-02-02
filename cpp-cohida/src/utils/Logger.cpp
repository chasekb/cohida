#include "utils/Logger.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace utils {

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::initialize(const std::string& log_level, const std::string& log_file) {
    // Create log directory if it doesn't exist
    fs::path log_path(log_file);
    if (!log_path.parent_path().empty()) {
        fs::create_directories(log_path.parent_path());
    }

    // Create console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    
    // Create file sink
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
    file_sink->set_level(spdlog::level::trace);
    
    // Create logger with multiple sinks
    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};
    logger_ = std::make_shared<spdlog::logger>("cohida_logger", sinks.begin(), sinks.end());
    logger_->set_level(spdlog::level::trace);
    logger_->flush_on(spdlog::level::err);
    
    set_pattern();
    set_level(log_level);
    
    LOG_INFO("Logger initialized successfully");
}

void Logger::set_level(const std::string& level) {
    spdlog::level::level_enum log_level = spdlog::level::info;
    
    if (level == "trace") {
        log_level = spdlog::level::trace;
    } else if (level == "debug") {
        log_level = spdlog::level::debug;
    } else if (level == "info") {
        log_level = spdlog::level::info;
    } else if (level == "warn") {
        log_level = spdlog::level::warn;
    } else if (level == "error") {
        log_level = spdlog::level::err;
    } else if (level == "critical") {
        log_level = spdlog::level::critical;
    }
    
    logger_->set_level(log_level);
    LOG_INFO("Log level set to: {}", level);
}

void Logger::set_pattern(const std::string& pattern) {
    logger_->set_pattern(pattern);
}

} // namespace utils