#include "config/Config.h"
#include "dotenv_loader.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace config {

Config &Config::get_instance() {
  static Config instance;
  return instance;
}

void Config::load(const std::string &filename) {
  if (std::filesystem::exists(filename)) {
    try {
      dotenv::init(filename);
    } catch (const std::exception &e) {
      // Fall back to environment variables
    }
  }

  load_from_env();
}

void Config::load_from_env() {
  // Load environment variables
  if (const char *api_key = std::getenv("COINBASE_API_KEY")) {
    config_["COINBASE_API_KEY"] = api_key;
  }
  if (const char *api_secret = std::getenv("COINBASE_API_SECRET")) {
    config_["COINBASE_API_SECRET"] = api_secret;
  }
  if (const char *api_passphrase = std::getenv("COINBASE_API_PASSPHRASE")) {
    config_["COINBASE_API_PASSPHRASE"] = api_passphrase;
  }

  if (const char *db_host = std::getenv("DB_HOST")) {
    config_["DB_HOST"] = db_host;
  }
  if (const char *db_port = std::getenv("DB_PORT")) {
    config_["DB_PORT"] = db_port;
  }
  if (const char *db_name = std::getenv("DB_NAME")) {
    config_["DB_NAME"] = db_name;
  }
  if (const char *db_user = std::getenv("DB_USER")) {
    config_["DB_USER"] = db_user;
  }
  if (const char *db_password = std::getenv("DB_PASSWORD")) {
    config_["DB_PASSWORD"] = db_password;
  }

  if (const char *log_level = std::getenv("LOG_LEVEL")) {
    config_["LOG_LEVEL"] = log_level;
  }

  // Default values for required fields
  if (!has_key("DB_HOST")) {
    config_["DB_HOST"] = "localhost";
  }
  if (!has_key("DB_PORT")) {
    config_["DB_PORT"] = "5432";
  }
  if (!has_key("DB_NAME")) {
    config_["DB_NAME"] = "coinbase_data";
  }
  if (!has_key("DB_USER")) {
    config_["DB_USER"] = "postgres";
  }
  if (!has_key("DB_PASSWORD")) {
    config_["DB_PASSWORD"] = "postgres";
  }
  if (!has_key("LOG_LEVEL")) {
    config_["LOG_LEVEL"] = "info";
  }
}

std::string Config::get_string(const std::string &key,
                               const std::string &default_value) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    return it->second;
  }
  return default_value; // Always return default value, never throw
}

int Config::get_int(const std::string &key, int default_value) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    try {
      return std::stoi(it->second);
    } catch (const std::invalid_argument &) {
      throw ConfigValueTypeException(key, "int");
    } catch (const std::out_of_range &) {
      throw ConfigValueTypeException(key, "int");
    }
  }
  return default_value;
}

double Config::get_double(const std::string &key, double default_value) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    try {
      return std::stod(it->second);
    } catch (const std::invalid_argument &) {
      throw ConfigValueTypeException(key, "double");
    } catch (const std::out_of_range &) {
      throw ConfigValueTypeException(key, "double");
    }
  }
  return default_value;
}

bool Config::get_bool(const std::string &key, bool default_value) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (value == "true" || value == "1" || value == "yes" || value == "on") {
      return true;
    } else if (value == "false" || value == "0" || value == "no" ||
               value == "off") {
      return false;
    }

    throw ConfigValueTypeException(key, "bool");
  }
  return default_value;
}

std::optional<std::string>
Config::get_optional_string(const std::string &key) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<int> Config::get_optional_int(const std::string &key) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    try {
      return std::stoi(it->second);
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

std::optional<double>
Config::get_optional_double(const std::string &key) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    try {
      return std::stod(it->second);
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

std::optional<bool> Config::get_optional_bool(const std::string &key) const {
  auto it = config_.find(key);
  if (it != config_.end()) {
    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (value == "true" || value == "1" || value == "yes" || value == "on") {
      return true;
    } else if (value == "false" || value == "0" || value == "no" ||
               value == "off") {
      return false;
    }
  }
  return std::nullopt;
}

bool Config::has_key(const std::string &key) const {
  return config_.find(key) != config_.end();
}

void Config::set(const std::string &key, const std::string &value) {
  config_[key] = value;
}

} // namespace config