#pragma once

#include <chrono>
#include <pqxx/params.hxx>
#include <pqxx/strconv.hxx>
#include <boost/decimal/decimal128_t.hpp>

namespace pqxx {

// Specialization for std::chrono::system_clock::time_point
template <>
struct nullness<std::chrono::system_clock::time_point, void> {
    static constexpr bool always_null = false;
};

// Specialization for models::Decimal (which is boost::decimal::decimal128)
template <>
struct nullness<boost::decimal::decimal128_t, void> {
    static constexpr bool always_null = false;
};

} // namespace pqxx
