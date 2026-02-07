#!/bin/bash

# Build script for C++ Coinbase Historical Data Retrieval Project

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check for required tools
check_tools() {
    print_info "Checking for required tools..."
    
    if ! command -v cmake &> /dev/null; then
        print_error "cmake not found. Please install CMake 3.20 or later."
        return 1
    fi
    
    if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
        print_error "C++ compiler not found. Please install g++ or clang++."
        return 1
    fi
    
    print_info "All required tools found"
    return 0
}

# Configure project
configure_project() {
    print_info "Configuring project..."
    
    mkdir -p build
    cd build
    
    if [ -z "$VCPKG_ROOT" ]; then
        print_warning "VCPKG_ROOT not set. Checking for vcpkg in current directory..."
        if [ -d "../vcpkg" ]; then
            export VCPKG_ROOT=$(pwd)/../vcpkg
            print_info "Using vcpkg from: $VCPKG_ROOT"
        else
            print_warning "vcpkg not found in current directory. Will attempt to download and install..."
            git clone https://github.com/microsoft/vcpkg.git ../vcpkg
            cd ../vcpkg
            ./bootstrap-vcpkg.sh
            cd ../build
            export VCPKG_ROOT=$(pwd)/../vcpkg
        fi
    fi
    
    cmake .. -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    
    print_info "Project configured successfully"
}

# Build project
build_project() {
    print_info "Building project..."
    
    cd build
    make -j$(nproc)
    
    print_info "Project built successfully"
}

# Run tests
run_tests() {
    print_info "Running tests..."
    
    cd build
    ctest -j$(nproc)
    
    print_info "All tests passed"
}

# Clean build
clean_build() {
    print_info "Cleaning build directory..."
    
    if [ -d "build" ]; then
        rm -rf build
    fi
    
    print_info "Build directory cleaned"
}

# Show usage information
show_usage() {
    echo "Usage: $0 [command]"
    echo
    echo "Commands:"
    echo "  all       - Configure, build, and run tests (default)"
    echo "  configure - Configure the project with CMake"
    echo "  build     - Build the project"
    echo "  test      - Run tests"
    echo "  clean     - Clean the build directory"
    echo "  help      - Show this help message"
}

# Main function
main() {
    if [ "$1" = "help" ]; then
        show_usage
        return 0
    fi
    
    print_info "Starting C++ Coinbase Historical Data Retrieval Project build"
    
    if ! check_tools; then
        return 1
    fi
    
    if [ "$1" = "clean" ]; then
        clean_build
        return 0
    fi
    
    if [ "$1" = "configure" ]; then
        configure_project
        return $?
    fi
    
    if [ "$1" = "build" ]; then
        if [ ! -d "build" ]; then
            print_warning "Build directory not found. Configuring project first..."
            configure_project || return $?
        fi
        build_project
        return $?
    fi
    
    if [ "$1" = "test" ]; then
        if [ ! -d "build" ]; then
            print_warning "Build directory not found. Configuring and building first..."
            configure_project || return $?
            build_project || return $?
        fi
        run_tests
        return $?
    fi
    
    # Default: all
    print_info "Running complete build (configure, build, test)..."
    configure_project || return $?
    build_project || return $?
    run_tests || return $?
    
    print_info "Build complete! Executable available at: build/bin/cohida"
    
    return 0
}

# Execute main function
main "$@"