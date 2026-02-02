#pragma once

#include <stdexcept>
#include <string>

namespace database {

class DbException : public std::runtime_error {
public:
    explicit DbException(const std::string& message)
        : std::runtime_error("Database error: " + message) {}
};

class DbConnectionException : public DbException {
public:
    explicit DbConnectionException(const std::string& message)
        : DbException("Connection failed: " + message) {}
};

class DbQueryException : public DbException {
public:
    explicit DbQueryException(const std::string& message)
        : DbException("Query failed: " + message) {}
};

class DbTransactionException : public DbException {
public:
    explicit DbTransactionException(const std::string& message)
        : DbException("Transaction failed: " + message) {}
};

class DbInvalidDataException : public DbException {
public:
    explicit DbInvalidDataException(const std::string& message)
        : DbException("Invalid data: " + message) {}
};

class DbConnectionPoolException : public DbException {
public:
    explicit DbConnectionPoolException(const std::string& message)
        : DbException("Connection pool error: " + message) {}
};

} // namespace database