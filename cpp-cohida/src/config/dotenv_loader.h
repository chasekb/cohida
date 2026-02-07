#ifndef DOTENV_LOADER_H
#define DOTENV_LOADER_H

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

namespace dotenv {

inline std::map<std::string, std::string> parse_env_file(const std::string& filename) {
    std::map<std::string, std::string> env_vars;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Find key-value separator
        size_t separator_pos = line.find('=');
        if (separator_pos == std::string::npos) {
            continue;  // Invalid line, skip
        }
        
        std::string key = line.substr(0, separator_pos);
        std::string value = line.substr(separator_pos + 1);
        
        // Trim whitespace from key and value
        size_t key_start = key.find_first_not_of(" \t");
        size_t key_end = key.find_last_not_of(" \t");
        if (key_start != std::string::npos && key_end != std::string::npos) {
            key = key.substr(key_start, key_end - key_start + 1);
        }
        
        size_t value_start = value.find_first_not_of(" \t");
        size_t value_end = value.find_last_not_of(" \t");
        if (value_start != std::string::npos && value_end != std::string::npos) {
            value = value.substr(value_start, value_end - value_start + 1);
        }
        
        // Remove quotes if present
        if (!value.empty()) {
            if ((value.front() == '"' && value.back() == '"') || 
                (value.front() == '\'' && value.back() == '\'')) {
                value = value.substr(1, value.size() - 2);
            }
        }
        
        if (!key.empty()) {
            env_vars[key] = value;
            // Also set in the process environment
            setenv(key.c_str(), value.c_str(), 1);
        }
    }
    
    return env_vars;
}

inline void init(const std::string& filename = ".env") {
    try {
        parse_env_file(filename);
    } catch (const std::exception&) {
        // If file not found or unreadable, just use existing environment variables
    }
}

} // namespace dotenv

#endif // DOTENV_LOADER_H