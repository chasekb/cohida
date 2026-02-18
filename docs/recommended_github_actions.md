# Recommended GitHub Actions Workflow

This document outlines the recommended GitHub Actions workflow for building and testing the C++ conversion of the cohida project.

## Workflow Overview

The recommended workflow uses a combination of `vcpkg` for dependency management and `podman-compose` (or standard `docker-compose`) for testing in a consistent environment.

## `.github/workflows/cpp-build.yml`

```yaml
name: C++ Build and Test

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v4

    - name: Set up CMake
      uses: jwlawrence/actions-setup-cmake@v1

    - name: Set up vcpkg
      run: |
        git clone https://github.com/microsoft/vcpkg.git
        ./vcpkg/bootstrap-vcpkg.sh
        echo "VCPKG_ROOT=$(pwd)/vcpkg" >> $GITHUB_ENV

    - name: Cache vcpkg dependencies
      uses: actions/cache@v3
      with:
        path: |
          ~/.cache/vcpkg
          ${{ env.VCPKG_ROOT }}/installed
          ${{ env.VCPKG_ROOT }}/vcpkg-tool
          ${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake
        key: ${{ runner.os }}-vcpkg-${{ hashFiles('cpp-cohida/vcpkg.json') }}

    - name: Build Project
      run: |
        cd cpp-cohida
        mkdir build
        cd build
        cmake .. -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
        make -j2

    - name: Run Unit Tests
      run: |
        cd cpp-cohida/build
        ./bin/cohida-unit-tests
```

## Multi-Container Testing (with Docker/Podman)

For testing that requires a database, it is recommended to use the `docker-compose.test.yml` configuration:

```yaml
  test-container:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Set up Docker Compose
      run: sudo apt-get update && sudo apt-get install -y docker-compose

    - name: Run tests in container
      run: |
        cd cpp-cohida
        docker-compose -f docker-compose.test.yml up --build --exit-code-from cohida-test-app
```

## Key Considerations

1. **OOM Prevention**: Use `make -j2` (or lower) to prevent out-of-memory errors on GitHub Actions runners, which typically have 7GB of RAM.
2. **Caching**: Caching `installed` directory of vcpkg is crucial as building dependencies from scratch can take 30+ minutes.
3. **Sanitizers**: Consider adding a separate job for building with AddressSanitizer and UndefinedBehaviorSanitizer enabled.
