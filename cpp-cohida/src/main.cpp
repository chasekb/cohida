#include "api/CoinbaseClient.h"
#include "config/Config.h"
#include "data/DataRetriever.h"
#include "data/FileWriter.h"
#include "database/DatabaseManager.h"
#include "models/DataPoint.h"
#include "utils/Logger.h"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace api;
using namespace config;
using namespace data;
using namespace database;
using namespace std;

// Helper to get formatted timestamp
string get_iso_time(const chrono::system_clock::time_point &tp) {
  auto time_t_val = chrono::system_clock::to_time_t(tp);
  std::tm tm_val = *std::gmtime(&time_t_val);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
  return string(buffer);
}

// Helper to parse date string
chrono::system_clock::time_point parse_date(const string &date_str) {
  std::istringstream ss(date_str);
  std::tm tm = {};
  ss >> std::get_time(&tm, "%Y-%m-%d");
  if (ss.fail()) {
    // Try with time
    ss.clear();
    ss.str(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  }
  return chrono::system_clock::from_time_t(std::mktime(&tm));
}

void setup_logging(bool verbose) {
  utils::Logger::initialize();
  utils::Logger::set_level(verbose ? "debug" : "info");
}

int main(int argc, char **argv) {
  CLI::App app{"Coinbase Historical Data Retrieval CLI"};
  argv = app.ensure_utf8(argv);

  bool verbose = false;
  app.add_flag("--verbose,-v", verbose, "Enable verbose logging");

  // Common options
  string output_dir = ".";
  app.add_option("--output-dir", output_dir, "Directory for output files");

  // --- Commands ---

  // 1. Test Command
  auto cmd_test =
      app.add_subcommand("test", "Test API and Database connections");
  cmd_test->callback([&]() {
    setup_logging(verbose);
    try {
      Config::get_instance().load();
      auto &config = Config::get_instance();

      LOG_INFO("Testing Coinbase API connection...");
      CoinbaseClient client(config.api_key(), config.api_secret(),
                            config.api_passphrase());
      if (client.test_connection()) {
        LOG_INFO("API Connection Successful");
      } else {
        LOG_ERROR("API Connection Failed");
      }

      LOG_INFO("Testing Database connection...");
      DatabaseManager db_manager;
      if (db_manager.test_connection()) {
        LOG_INFO("Database Connection Successful");
      } else {
        LOG_ERROR("Database Connection Failed");
      }
    } catch (const exception &e) {
      LOG_ERROR("Test failed: {}", e.what());
    }
  });

  // 2. Symbols Command
  auto cmd_symbols = app.add_subcommand("symbols", "List available symbols");
  string symbols_output;
  cmd_symbols->add_option("--output,-o", symbols_output, "Output JSON file");
  cmd_symbols->callback([&]() {
    setup_logging(verbose);
    try {
      Config::get_instance().load();
      auto &config = Config::get_instance();
      CoinbaseClient client(config.api_key(), config.api_secret(),
                            config.api_passphrase());

      auto symbols = client.get_available_symbols();
      LOG_INFO("Found {} symbols", symbols.size());

      if (!symbols_output.empty()) {
        FileWriter::write_json(symbols, symbols_output);
      } else {
        for (size_t i = 0; i < min(symbols.size(), static_cast<size_t>(10));
             ++i) {
          cout << symbols[i].symbol << " (" << symbols[i].display_name << ")"
               << endl;
        }
        if (symbols.size() > 10)
          cout << "... and " << (symbols.size() - 10) << " more" << endl;
      }
    } catch (const exception &e) {
      LOG_ERROR("Error fetching symbols: {}", e.what());
    }
  });

  // 3. Info Command
  auto cmd_info = app.add_subcommand("info", "Get symbol information");
  string info_symbol;
  cmd_info->add_option("--symbol,-s", info_symbol, "Symbol (e.g., BTC-USD)")
      ->required();
  cmd_info->callback([&]() {
    setup_logging(verbose);
    try {
      Config::get_instance().load();
      auto &config = Config::get_instance();
      CoinbaseClient client(config.api_key(), config.api_secret(),
                            config.api_passphrase());

      auto info = client.get_symbol_info(info_symbol);
      cout << "Symbol: " << info.symbol << endl;
      cout << "Display Name: " << info.display_name << endl;
      cout << "Status: " << info.status << endl;
      cout << "Base Currency: " << info.base_currency << endl;
      cout << "Quote Currency: " << info.quote_currency << endl;
    } catch (const exception &e) {
      LOG_ERROR("Error fetching info for {}: {}", info_symbol, e.what());
    }
  });

  // 4. Retrieve Command
  auto cmd_retrieve =
      app.add_subcommand("retrieve", "Retrieve historical data");
  string ret_symbol, ret_start, ret_end, ret_output;
  int ret_granularity = 3600;
  cmd_retrieve->add_option("--symbol,-s", ret_symbol, "Symbol (e.g., BTC-USD)")
      ->required();
  cmd_retrieve->add_option("--start", ret_start, "Start date (YYYY-MM-DD)")
      ->required();
  cmd_retrieve->add_option("--end", ret_end, "End date (YYYY-MM-DD)")
      ->required();
  cmd_retrieve->add_option("--granularity,-g", ret_granularity,
                           "Granularity in seconds (default: 3600)");
  cmd_retrieve->add_option("--output,-o", ret_output,
                           "Output file (CSV or JSON)");

  cmd_retrieve->callback([&]() {
    setup_logging(verbose);
    try {
      Config::get_instance().load();

      auto start_tp = parse_date(ret_start);
      auto end_tp = parse_date(ret_end);

      DataRetriever retriever;
      auto result = retriever.retrieve_historical_data(ret_symbol, start_tp,
                                                       end_tp, ret_granularity);

      if (result.success) {
        LOG_INFO("Successfully retrieved {} data points for {}",
                 result.data_points.size(), ret_symbol);

        // Save to DB
        DatabaseManager db(ret_granularity);
        db.write_data(result.data_points);
        LOG_INFO("Data written to database");

        // Check file output
        if (!ret_output.empty()) {
          if (ret_output.ends_with(".json")) {
            FileWriter::write_json(result.data_points, ret_output);
          } else {
            FileWriter::write_csv(result.data_points, ret_output);
          }
        }
      } else {
        LOG_ERROR("Data retrieval failed: {}",
                  result.error_message.value_or("Unknown error"));
      }
    } catch (const exception &e) {
      LOG_ERROR("Error retrieving data: {}", e.what());
    }
  });

  // 5. Retrieve All Command
  auto cmd_retrieve_all =
      app.add_subcommand("retrieve-all", "Retrieve ALL historical data");
  string ra_symbol;
  int ra_granularity = 86400; // Default to daily
  cmd_retrieve_all
      ->add_option("--symbol,-s", ra_symbol, "Symbol (e.g., BTC-USD)")
      ->required();
  cmd_retrieve_all->add_option("--granularity,-g", ra_granularity,
                               "Granularity in seconds (default: 86400)");

  cmd_retrieve_all->callback([&]() {
    setup_logging(verbose);
    try {
      Config::get_instance().load();
      DataRetriever retriever;
      // Note: This might take a long time and handles chunking internally
      auto result =
          retriever.retrieve_all_historical_data(ra_symbol, ra_granularity);

      if (result.success) {
        LOG_INFO("Finished retrieving all data for {}. Total points: {}",
                 ra_symbol, result.data_points.size());
        // Save to DB
        DatabaseManager db(ra_granularity);
        db.write_data(result.data_points);
        LOG_INFO("All data written to database");
      } else {
        LOG_ERROR("Failed to retrieve all data: {}",
                  result.error_message.value_or("Unknown error"));
      }

    } catch (const exception &e) {
      LOG_ERROR("Error in retrieve-all: {}", e.what());
    }
  });

  // 6. Read Command
  auto cmd_read = app.add_subcommand("read", "Read data from database");
  string read_symbol, read_start, read_end, read_output;
  int read_granularity = 3600;
  cmd_read->add_option("--symbol,-s", read_symbol, "Symbol (e.g., BTC-USD)")
      ->required();
  cmd_read->add_option("--start", read_start, "Start date (YYYY-MM-DD)")
      ->required();
  cmd_read->add_option("--end", read_end, "End date (YYYY-MM-DD)")->required();
  cmd_read->add_option("--granularity,-g", read_granularity,
                       "Granularity in seconds (default: 3600)");
  cmd_read->add_option("--output,-o", read_output, "Output file (CSV or JSON)");

  cmd_read->callback([&]() {
    setup_logging(verbose);
    try {
      Config::get_instance().load();
      auto start_tp = parse_date(read_start);
      auto end_tp = parse_date(read_end);

      DatabaseManager db(read_granularity);
      auto data = db.read_data(read_symbol, start_tp, end_tp);

      LOG_INFO("Read {} records from database", data.size());

      if (!read_output.empty()) {
        if (read_output.ends_with(".json")) {
          FileWriter::write_json(data, read_output);
        } else {
          FileWriter::write_csv(data, read_output);
        }
      } else {
        // Print first 5
        for (size_t i = 0; i < min(data.size(), static_cast<size_t>(5)); ++i) {
          cout << get_iso_time(data[i].timestamp) << " - "
               << data[i].close_price << endl;
        }
      }
    } catch (const exception &e) {
      LOG_ERROR("Error reading data: {}", e.what());
    }
  });

  CLI11_PARSE(app, argc, argv);
  return 0;
}
