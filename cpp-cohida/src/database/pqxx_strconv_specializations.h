#pragma once

#include <chrono>
#include <pqxx/params.hxx>
#include <pqxx/strconv.hxx>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace pqxx {

// Specialization for std::chrono::system_clock::time_point
template <>
struct nullness<std::chrono::system_clock::time_point, void> {
    static constexpr bool always_null = false;
};

// Specialization for models::Decimal (which is boost::multiprecision::cpp_dec_float_100)
template <>
struct nullness<boost::multiprecision::number<boost::multiprecision::backends::cpp_dec_float<100, int, void>, boost::multiprecision::et_on>, void> {
    static constexpr bool always_null = false;
};

} // namespace pqxx