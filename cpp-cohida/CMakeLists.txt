cmake_minimum_required(VERSION 3.20)

# Project configuration
project(cohida
    VERSION 1.0.0
    DESCRIPTION "Coinbase Historical Data Retrieval and ML Pipeline"
    LANGUAGES CXX
)

# C++20 standard requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX /EHsc)
    add_compile_options(/fsanitize=address)
    add_link_options(/fsanitize=address)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(include)
include_directories(src)

# Find dependencies using Vcpkg
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
find_package(libpqxx REQUIRED)
find_package(CLI11 REQUIRED)
find_package(spdlog REQUIRED)
find_package(date REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Boost REQUIRED)
find_package(boost_decimal CONFIG REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(fmt REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Optional ML dependencies
find_package(xgboost QUIET)
find_package(LightGBM QUIET)

if(xgboost_FOUND)
    message(STATUS "XGBoost found - enabling ML training with XGBoost")
else()
    message(STATUS "XGBoost not found - XGBoost training disabled")
endif()

if(LightGBM_FOUND)
    message(STATUS "LightGBM found - enabling ML training with LightGBM")
else()
    message(STATUS "LightGBM not found - LightGBM training disabled")
endif()

# Targets

# Core library
add_library(cohida-core
    src/config/Config.cpp
    src/api/CoinbaseClient.cpp
    src/models/DataPoint.cpp
    src/utils/Logger.cpp
    src/database/DatabaseManager.cpp
    src/data/DataRetriever.cpp
    src/data/FileWriter.cpp
    src/ml/TechnicalIndicators.cpp
    src/ml/FeatureEngineer.cpp
    src/ml/Preprocessor.cpp
    src/ml/DataSplitter.cpp
    src/ml/ModelTrainer.cpp
    src/ml/ModelRegistry.cpp
)

target_include_directories(cohida-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cohida-core
    PUBLIC
        nlohmann_json::nlohmann_json
        date::date
        Eigen3::Eigen
        Boost::decimal
        ${Boost_LIBRARIES}
        fmt::fmt
    PRIVATE
        CURL::libcurl
        libpqxx::pqxx
        CLI11::CLI11
        spdlog::spdlog
        jwt-cpp::jwt-cpp
)

# Conditional ML library linking
if(xgboost_FOUND)
    target_link_libraries(cohida-core PRIVATE xgboost::xgboost)
    target_compile_definitions(cohida-core PRIVATE COHIDA_HAS_XGBOOST)
endif()

if(LightGBM_FOUND)
    target_link_libraries(cohida-core PRIVATE LightGBM::LightGBM)
    target_compile_definitions(cohida-core PRIVATE COHIDA_HAS_LIGHTGBM)
endif()

# Main executable
add_executable(cohida
    src/main.cpp
)

target_link_libraries(cohida
    PRIVATE
        cohida-core
        CLI11::CLI11
        spdlog::spdlog
)

# Tests
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Unit tests
add_executable(cohida-unit-tests
    tests/unit/test_coinbase_client.cpp
    tests/unit/test_coinbase_auth.cpp
    tests/unit/test_database_manager.cpp
    tests/unit/test_data_point.cpp
    tests/unit/test_data_retriever.cpp
    tests/unit/test_technical_indicators.cpp
    tests/unit/test_feature_engineer.cpp
    tests/unit/test_preprocessor.cpp
    tests/unit/test_data_splitter.cpp
)

target_link_libraries(cohida-unit-tests
    PRIVATE
        cohida-core
        GTest::gtest_main
        GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(cohida-unit-tests)

# Examples
add_subdirectory(examples)