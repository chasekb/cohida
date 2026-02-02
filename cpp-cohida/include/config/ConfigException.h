#pragma once

#include <stdexcept>
#include <string>

namespace config {

class ConfigException : public std::runtime_error {
public:
    explicit ConfigException(const std::string& message)
        : std::runtime_error("Config error: " + message) {}
};

class ConfigFileNotFoundException : public ConfigException {
public:
    explicit ConfigFileNotFoundException(const std::string& filename)
        : ConfigException("File not found: " + filename) {}
};

class ConfigKeyNotFoundException : public ConfigException {
public:
    explicit ConfigKeyNotFoundException(const std::string& key)
        : ConfigException("Key not found: " + key) {}
};

class ConfigValueTypeException : public ConfigException {
public:
    explicit ConfigValueTypeException(const std::string& key, const std::string& expected_type)
        : ConfigException("Key '" + key + "' has invalid type. Expected: " + expected_type) {}
};

} // namespace config