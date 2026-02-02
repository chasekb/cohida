add_executable(test_config test_config.cpp)

target_link_libraries(test_config
    PRIVATE
        cohida-core
)

# Set output directory for examples
set_target_properties(test_config PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)