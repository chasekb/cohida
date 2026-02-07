add_executable(test_config test_config.cpp)
add_executable(test_coinbase_client test_coinbase_client.cpp)
add_executable(test_coinbase_auth test_coinbase_auth.cpp)

target_link_libraries(test_config
    PRIVATE
        cohida-core
)

target_link_libraries(test_coinbase_client
    PRIVATE
        cohida-core
)

target_link_libraries(test_coinbase_auth
    PRIVATE
        cohida-core
)

# Set output directory for examples
set_target_properties(test_config test_coinbase_client test_coinbase_auth PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)
