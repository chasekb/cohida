#pragma once

#include <stdexcept>
#include <string>

namespace api {

class ApiException : public std::runtime_error {
public:
    explicit ApiException(const std::string& message)
        : std::runtime_error("API error: " + message) {}
};

class ApiConnectionException : public ApiException {
public:
    explicit ApiConnectionException(const std::string& message)
        : ApiException("Connection failed: " + message) {}
};

class ApiAuthenticationException : public ApiException {
public:
    explicit ApiAuthenticationException(const std::string& message)
        : ApiException("Authentication failed: " + message) {}
};

class ApiRateLimitException : public ApiException {
public:
    explicit ApiRateLimitException(const std::string& message)
        : ApiException("Rate limit exceeded: " + message) {}
};

class ApiRequestException : public ApiException {
public:
    ApiRequestException(int status_code, const std::string& message)
        : ApiException("HTTP " + std::to_string(status_code) + ": " + message),
          status_code_(status_code) {}

    int status_code() const { return status_code_; }

private:
    int status_code_;
};

class ApiResponseException : public ApiException {
public:
    explicit ApiResponseException(const std::string& message)
        : ApiException("Invalid response: " + message) {}
};

} // namespace api