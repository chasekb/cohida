FROM ubuntu:22.04

WORKDIR /app

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /app/vcpkg
WORKDIR /app/vcpkg
RUN ./bootstrap-vcpkg.sh

# Copy project files
WORKDIR /app
COPY . .

# Create build directory
RUN mkdir -p build

# Install dependencies using vcpkg
RUN ./vcpkg/vcpkg install

# Build the project
WORKDIR /app/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=/app/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN make -j$(nproc)

# Run tests
RUN ctest -j$(nproc)

# Default command
CMD ["./bin/cohida", "--help"]