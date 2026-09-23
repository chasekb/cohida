#pragma once

#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>

namespace utils {

struct FailureRecord {
  std::string run_timestamp;
  std::string symbol;
  int granularity;
  std::string window_start;
  std::string window_end;
  std::string stage;
  std::string error_category;
  std::string error_summary;
  std::string retry_outcome;
  std::string symbol_outcome;

  nlohmann::json to_json() const {
    return {
        {"run_timestamp", run_timestamp},
        {"symbol", symbol},
        {"granularity", granularity},
        {"window_start", window_start},
        {"window_end", window_end},
        {"stage", stage},
        {"error_category", error_category},
        {"error_summary", error_summary},
        {"retry_outcome", retry_outcome},
        {"symbol_outcome", symbol_outcome},
    };
  }
};

inline std::string failure_log_timestamp(
    const std::chrono::system_clock::time_point &time) {
  const auto time_t_value = std::chrono::system_clock::to_time_t(time);
  std::tm tm_value{};
  gmtime_r(&time_t_value, &tm_value);
  std::ostringstream output;
  output << std::put_time(&tm_value, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

inline std::string sanitize_failure_summary(std::string summary) {
  static const std::regex secret_pattern(
      R"((api[_-]?key|api[_-]?secret|passphrase|password|token|authorization)\s*[:=]\s*[^,;\s]+)",
      std::regex::icase);
  summary = std::regex_replace(summary, secret_pattern, "$1=[REDACTED]");
  for (char &character : summary) {
    if (std::iscntrl(static_cast<unsigned char>(character)) &&
        character != '\n' && character != '\t') {
      character = ' ';
    }
  }
  return summary.substr(0, 512);
}

} // namespace utils
