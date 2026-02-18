#include "api/CoinbaseClient.h"
#include "config/Config.h"
#include "data/DataRetriever.h"
#include "data/FileWriter.h"
#include "database/DatabaseManager.h"
#include "ml/FeatureEngineer.h"
#include "ml/ModelRegistry.h"
#include "ml/ModelTrainer.h"
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
      i Config::get_instance().load();
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
  bool symbols_list = false;
  cmd_symbols->add_option("--output,-o", symbols_output, "Output JSON file");
  cmd_symbols->add_flag("--list,-l", symbols_list,
                        "Print all symbols as a simple list");
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
      } else if (symbols_list) {
        for (const auto &s : symbols) {
          cout << s.symbol << endl;
        }
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
      if (info) {
        cout << "Symbol: " << info->symbol << endl;
        cout << "Display Name: " << info->display_name << endl;
        cout << "Status: " << info->status << endl;
        cout << "Base Currency: " << info->base_currency << endl;
        cout << "Quote Currency: " << info->quote_currency << endl;
      } else {
        LOG_ERROR("Symbol info not found for {}", info_symbol);
      }
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
        LOG_ERROR("Data retrieval failed: {}", result.error_message.empty()
                                                   ? "Unknown error"
                                                   : result.error_message);
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
                  result.error_message.empty() ? "Unknown error"
                                               : result.error_message);
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

  // 7. ML Train Command
  auto cmd_ml_train = app.add_subcommand("ml-train", "Train ML model");
  string ml_symbol, ml_start, ml_end, ml_model_type = "xgboost",
                                      ml_task_type = "classification",
                                      ml_registry_path = "./models";
  int ml_granularity = 3600;
  cmd_ml_train->add_option("--symbol,-s", ml_symbol, "Symbol (e.g., BTC-USD)")
      ->required();
  cmd_ml_train->add_option("--start", ml_start, "Start date (YYYY-MM-DD)");
  cmd_ml_train->add_option("--end", ml_end, "End date (YYYY-MM-DD)");
  cmd_ml_train->add_option("--granularity,-g", ml_granularity,
                           "Granularity in seconds (default: 3600)");
  cmd_ml_train->add_option("--model-type", ml_model_type,
                           "Model type: xgboost, lightgbm (default: xgboost)");
  cmd_ml_train->add_option("--task-type", ml_task_type,
                           "Task type: classification, regression");
  cmd_ml_train->add_option("--registry", ml_registry_path,
                           "Model registry path (default: ./models)");

  cmd_ml_train->callback([&]() {
    setup_logging(verbose);
    try {
      Config::get_instance().load();

      // Read data from database
      DatabaseManager db(ml_granularity);
      chrono::system_clock::time_point start_tp, end_tp;
      if (!ml_start.empty())
        start_tp = parse_date(ml_start);
      else
        start_tp = chrono::system_clock::now() - chrono::hours(24 * 365);
      if (!ml_end.empty())
        end_tp = parse_date(ml_end);
      else
        end_tp = chrono::system_clock::now();

      auto data = db.read_data(ml_symbol, start_tp, end_tp);
      LOG_INFO("Loaded {} records for training", data.size());

      if (data.size() < 100) {
        LOG_ERROR("Insufficient data for training (need at least 100 rows, "
                  "got {})",
                  data.size());
        return;
      }

      // Build features
      ml::FeatureEngineer fe;
      auto features = fe.build_features(data);
      LOG_INFO("Feature matrix: {} rows x {} columns", features.rows(),
               features.cols());

      // Create binary target (price went up)
      Eigen::VectorXd target(features.rows());
      int close_idx = features.column_index("close");
      if (close_idx < 0) {
        LOG_ERROR("Cannot find close price column");
        return;
      }
      for (int i = 0; i < features.rows() - 1; ++i) {
        target(i) =
            (features.data(i + 1, close_idx) > features.data(i, close_idx))
                ? 1.0
                : 0.0;
      }
      target(features.rows() - 1) = 0.0;

      // Train
      ml::TrainingConfig train_cfg;
      train_cfg.model_type = ml_model_type;
      train_cfg.task_type = ml_task_type;

      ml::ModelTrainer trainer(train_cfg);
      auto result =
          trainer.train(features.data, target, features.column_names,
                        [](const std::string &msg) { LOG_INFO("  {}", msg); });

      if (result.success) {
        // Save to registry
        ml::ModelRegistry registry(ml_registry_path);
        ml::ModelMetadata metadata;
        metadata.model_id =
            ml::ModelRegistry::generate_model_id(ml_model_type, ml_symbol);
        metadata.model_type = ml_model_type;
        metadata.symbol = ml_symbol;
        metadata.granularity = to_string(ml_granularity);
        metadata.feature_names = result.feature_names;
        metadata.metrics = result.metrics;

        auto now = chrono::system_clock::now();
        metadata.created_at = get_iso_time(now);

        registry.save_model(result.model_data, metadata);

        cout << "Model trained and saved: " << metadata.model_id << endl;
        cout << "Training time: " << result.training_time_seconds << "s"
             << endl;
        for (const auto &[k, v] : result.metrics)
          cout << "  " << k << ": " << v << endl;
      } else {
        LOG_ERROR("Training failed: {}", result.error_message);
      }
    } catch (const exception &e) {
      LOG_ERROR("ML training error: {}", e.what());
    }
  });

  // 8. ML Models Command
  auto cmd_ml_models =
      app.add_subcommand("ml-models", "List trained ML models");
  string ml_list_symbol, ml_list_type, ml_list_registry = "./models";
  cmd_ml_models->add_option("--symbol,-s", ml_list_symbol, "Filter by symbol");
  cmd_ml_models->add_option("--model-type", ml_list_type,
                            "Filter by model type");
  cmd_ml_models->add_option("--registry", ml_list_registry,
                            "Model registry path");

  cmd_ml_models->callback([&]() {
    setup_logging(verbose);
    try {
      ml::ModelRegistry registry(ml_list_registry);
      auto models = registry.list_models(ml_list_symbol, ml_list_type);

      if (models.empty()) {
        cout << "No models found." << endl;
        return;
      }

      cout << "Found " << models.size() << " model(s):" << endl;
      cout << string(60, '-') << endl;
      for (const auto &m : models) {
        cout << "  ID: " << m.model_id << endl;
        cout << "  Type: " << m.model_type << endl;
        cout << "  Symbol: " << m.symbol << endl;
        cout << "  Created: " << m.created_at << endl;
        for (const auto &[k, v] : m.metrics)
          cout << "  " << k << ": " << v << endl;
        cout << string(60, '-') << endl;
      }
    } catch (const exception &e) {
      LOG_ERROR("Error listing models: {}", e.what());
    }
  });

  // 9. ML Info Command
  auto cmd_ml_info = app.add_subcommand("ml-info", "Show ML model details");
  string ml_info_id, ml_info_registry = "./models";
  cmd_ml_info->add_option("--model-id", ml_info_id, "Model ID")->required();
  cmd_ml_info->add_option("--registry", ml_info_registry,
                          "Model registry path");

  cmd_ml_info->callback([&]() {
    setup_logging(verbose);
    try {
      ml::ModelRegistry registry(ml_info_registry);
      auto metadata = registry.get_metadata(ml_info_id);

      cout << "Model: " << metadata.model_id << endl;
      cout << "Type: " << metadata.model_type << endl;
      cout << "Symbol: " << metadata.symbol << endl;
      cout << "Granularity: " << metadata.granularity << endl;
      cout << "Created: " << metadata.created_at << endl;
      cout << "Version: " << metadata.version << endl;

      cout << "\nMetrics:" << endl;
      for (const auto &[k, v] : metadata.metrics)
        cout << "  " << k << ": " << v << endl;

      cout << "\nHyperparameters:" << endl;
      for (const auto &[k, v] : metadata.hyperparameters)
        cout << "  " << k << ": " << v << endl;

      cout << "\nFeatures (" << metadata.feature_names.size() << "):" << endl;
      for (size_t i = 0;
           i < min(metadata.feature_names.size(), static_cast<size_t>(20));
           ++i) {
        cout << "  " << metadata.feature_names[i] << endl;
      }
      if (metadata.feature_names.size() > 20) {
        cout << "  ... and " << (metadata.feature_names.size() - 20) << " more"
             << endl;
      }
    } catch (const exception &e) {
      LOG_ERROR("Error getting model info: {}", e.what());
    }
  });

  CLI11_PARSE(app, argc, argv);
  return 0;
}
