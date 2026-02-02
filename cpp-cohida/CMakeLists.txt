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
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Weverything)
    endif()
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
find_package(dotenv-cpp REQUIRED)
find_package(date REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(ta-lib REQUIRED)

# Targets

# Core library
add_library(cohida-core
    src/config/Config.cpp
    src/api/CoinbaseClient.cpp
    src/database/DatabaseManager.cpp
    src/models/DataPoint.cpp
    src/models/SymbolInfo.cpp
    src/models/Validation.cpp
    src/data/DataRetriever.cpp
    src/data/FileWriter.cpp
    src/ml/TechnicalIndicators.cpp
    src/ml/FeatureEngineer.cpp
    src/ml/Preprocessor.cpp
    src/ml/ModelTrainer.cpp
    src/ml/ModelRegistry.cpp
    src/utils/Logger.cpp
    src/utils/Retry.cpp
    src/utils/RateLimiter.cpp
)

target_include_directories(cohida-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cohida-core
    PRIVATE
        nlohmann_json::nlohmann_json
        CURL::libcurl
        libpqxx::pqxx
        CLI11::CLI11
        spdlog::spdlog
        dotenv-cpp::dotenv-cpp
        date::date
        Eigen3::Eigen
        ta-lib::ta-lib
)

# Main executable
add_executable(cohida
    src/main.cpp
)

target_link_libraries(cohida
    PRIVATE
        cohida-core
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
    tests/unit/test_config.cpp
    tests/unit/test_coinbase_client.cpp
    tests/unit/test_database_manager.cpp
    tests/unit/test_data_point.cpp
    tests/unit/test_data_retriever.cpp
)

target_link_libraries(cohida-unit-tests
    PRIVATE
        cohida-core
        GTest::gtest_main
        GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(cohida-unit-tests)

# Integration tests
add_executable(cohida-integration-tests
    tests/integration/test_api_integration.cpp
    tests/integration/test_database_integration.cpp
)

target_link_libraries(cohida-integration-tests
    PRIVATE
        cohida-core
        GTest::gtest_main
        GTest::gmock_main
)

gtest_discover_tests(cohida-integration-tests)

# End-to-end tests
add_executable(cohida-e2e-tests
    tests/e2e/test_workflow.cpp
)

target_link_libraries(cohida-e2e-tests
    PRIVATE
        cohida-core
        GTest::gtest_main
        GTest::gmock_main
)

gtest_discover_tests(cohida-e2e-tests)

# Examples
add_subdirectory(examples)