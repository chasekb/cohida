#!/bin/bash

# Test script for C++ Coinbase Historical Data project

# Function to print messages
print_message() {
    echo "=============================================="
    echo "$1"
    echo "=============================================="
}

# Function to clean up resources
cleanup() {
    print_message "Cleaning up resources"
    podman-compose -f docker-compose.test.yml down
    rm -f test.log
    if [ -d "test_outputs" ]; then
        rm -rf test_outputs
    fi
}

# Handle script termination
trap cleanup EXIT INT TERM

print_message "Starting C++ Coinbase Historical Data project tests"

# Check if Podman is installed
if ! command -v podman &> /dev/null; then
    echo "Podman is not installed. Please install Podman first."
    exit 1
fi

# Check if Podman Compose is installed
if ! command -v podman-compose &> /dev/null; then
    echo "Podman Compose is not installed. Please install podman-compose first."
    exit 1
fi

# Create test outputs directory
mkdir -p test_outputs

# Build and run tests
print_message "Building test containers"
if ! podman-compose -f docker-compose.test.yml up --build -d; then
    echo "Failed to start test containers"
    exit 1
fi

print_message "Waiting for test containers to be healthy"
if ! podman-compose -f docker-compose.test.yml wait test-app; then
    echo "Test containers failed to start successfully"
    exit 1
fi

print_message "Running tests"
if ! podman-compose -f docker-compose.test.yml exec -T test-app ./bin/cohida-unit-tests; then
    echo "Tests failed"
    exit 1
fi

print_message "Tests passed successfully"

# Get test logs
print_message "Copying test logs"
podman cp cpp-cohida-test:/app/test.log test.log
echo "Test log copied to test.log"

# Clean up
cleanup

print_message "All tests completed successfully"

exit 0
