#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include "ConfigException.h"

namespace config {

class Config {
public:
    static Config& get_instance();
    
    void load(const std::string& filename = ".env");
    void load_from_env();
    
    std::string get_string(const std::string& key, const std::string& default_value = "") const;
    int get_int(const std::string& key, int default_value = 0) const;
    double get_double(const std::string& key, double default_value = 0.0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;
    
    std::optional<std::string> get_optional_string(const std::string& key) const;
    std::optional<int> get_optional_int(const std::string& key) const;
    std::optional<double> get_optional_double(const std::string& key) const;
    std::optional<bool> get_optional_bool(const std::string& key) const;
    
    bool has_key(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    
    const std::unordered_map<std::string, std::string>& get_all() const {
        return config_;
    }
    
private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    std::unordered_map<std::string, std::string> config_;
};

} // namespace config