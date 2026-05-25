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

    auto trim = [](std::string value) {
        const auto start = value.find_first_not_of(" \t");
        if (start == std::string::npos) {
            return std::string{};
        }
        const auto end = value.find_last_not_of(" \t");
        return value.substr(start, end - start + 1);
    };

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

        std::string key = trim(line.substr(0, separator_pos));
        std::string value = trim(line.substr(separator_pos + 1));

        // Support multiline quoted values (for PEM/private-key style secrets)
        if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
            const char quote = value.front();
            value.erase(value.begin());

            bool closed = false;
            while (true) {
                if (!value.empty() && value.back() == quote) {
                    value.pop_back();
                    closed = true;
                    break;
                }

                std::string continuation;
                if (!std::getline(file, continuation)) {
                    break;
                }
                value.push_back('\n');
                value += continuation;
            }

            if (!closed) {
                // Leave the accumulated value as-is; callers can validate it.
            }
        } else if (!value.empty() &&
                   ((value.front() == '"' && value.back() == '"') ||
                    (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
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