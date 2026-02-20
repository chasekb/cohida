#pragma once

#include "ConfigException.h"
#include <optional>
#include <string>
#include <unordered_map>

namespace config {

class Config {
public:
  static Config &get_instance();

  void load(const std::string &filename = ".env");
  void load_from_env();

  std::string get_string(const std::string &key,
                         const std::string &default_value = "") const;
  int get_int(const std::string &key, int default_value = 0) const;
  double get_double(const std::string &key, double default_value = 0.0) const;
  bool get_bool(const std::string &key, bool default_value = false) const;

  std::optional<std::string> get_optional_string(const std::string &key) const;
  std::optional<int> get_optional_int(const std::string &key) const;
  std::optional<double> get_optional_double(const std::string &key) const;
  std::optional<bool> get_optional_bool(const std::string &key) const;

  bool has_key(const std::string &key) const;
  void set(const std::string &key, const std::string &value);

  const std::unordered_map<std::string, std::string> &get_all() const {
    return config_;
  }

  // API credentials
  std::string api_key() const { return get_string("COINBASE_API_KEY", ""); }

  std::string api_secret() const {
    return get_string("COINBASE_API_SECRET", "");
  }

  std::string api_passphrase() const {
    return get_string("COINBASE_API_PASSPHRASE", "");
  }

  bool api_credentials_valid() const {
    return !api_key().empty() && !api_secret().empty();
  }

  // API settings
  bool sandbox_mode() const { return get_bool("COINBASE_SANDBOX_MODE", false); }

  int api_timeout() const { return get_int("COINBASE_API_TIMEOUT", 30); }

  int api_max_retries() const { return get_int("COINBASE_API_MAX_RETRIES", 3); }

  int api_retry_delay() const {
    return get_int("COINBASE_API_RETRY_DELAY", 1000);
  }

  // Database settings
  std::string db_host() const { return get_string("DB_HOST", "localhost"); }

  int db_port() const { return get_int("DB_PORT", 5432); }

  std::string db_name() const { return get_string("DB_NAME", "coinbase_data"); }

  std::string db_user() const { return get_string("DB_USER", "postgres"); }

  std::string db_password() const {
    return get_string("DB_PASSWORD", "postgres");
  }

  std::string db_schema() const { return get_string("DB_SCHEMA", "public"); }

  std::string db_table() const {
    return get_string("DB_TABLE", "crypto_prices");
  }

private:
  Config() = default;
  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

  std::unordered_map<std::string, std::string> config_;
};

} // namespace config