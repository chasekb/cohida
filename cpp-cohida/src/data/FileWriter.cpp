#include "data/FileWriter.h"
#include "utils/Logger.h"
#include <boost/decimal/decimal128_t.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>

namespace data {

using json = nlohmann::json;

void FileWriter::write_csv(const std::vector<models::CryptoPriceData> &data,
                           const std::string &filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file for writing: {}", filename);
    throw std::runtime_error("Could not open file " + filename);
  }

  // Write header
  file << "timestamp,symbol,open,high,low,close,volume\n";

  // Write data
  for (const auto &point : data) {
    // Convert timestamp to ISO 8601 string or similar if needed,
    // or just use seconds since epoch for simplicity in CSV unless formatted.
    // For better UX, let's output ISO string if possible, but Date library
    // might be needed. For now, let's use the decimal values directly. Note:
    // boost::decimal output might need specific formatting.

    auto time_t_val = std::chrono::system_clock::to_time_t(point.timestamp);
    std::tm tm_val = *std::gmtime(&time_t_val);
    char time_buffer[32];
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%SZ",
                  &tm_val);

    file << time_buffer << "," << point.symbol << "," << point.open_price << ","
         << point.high_price << "," << point.low_price << ","
         << point.close_price << "," << point.volume << "\n";
  }

  file.close();
  LOG_INFO("Successfully wrote {} records to {}", data.size(), filename);
}

void FileWriter::write_json(const std::vector<models::CryptoPriceData> &data,
                            const std::string &filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file for writing: {}", filename);
    throw std::runtime_error("Could not open file " + filename);
  }

  json j = json::array();
  for (const auto &point : data) {
    j.push_back(point.to_json());
  }

  file << j.dump(4); // Pretty print with 4 spaces indent
  file.close();
  LOG_INFO("Successfully wrote {} records to {}", data.size(), filename);
}

void FileWriter::write_json(const models::SymbolInfo &info,
                            const std::string &filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file for writing: {}", filename);
    throw std::runtime_error("Could not open file " + filename);
  }

  json j = info.to_json();
  file << j.dump(4);
  file.close();
  LOG_INFO("Successfully wrote symbol info for {} to {}", info.symbol,
           filename);
}

void FileWriter::write_json(const std::vector<models::SymbolInfo> &infos,
                            const std::string &filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file for writing: {}", filename);
    throw std::runtime_error("Could not open file " + filename);
  }

  json j = json::array();
  for (const auto &info : infos) {
    j.push_back(info.to_json());
  }

  file << j.dump(4);
  file.close();
  LOG_INFO("Successfully wrote {} symbol infos to {}", infos.size(), filename);
}

} // namespace data
