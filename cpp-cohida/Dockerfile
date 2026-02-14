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
    bison \
    flex \
    autoconf \
    automake \
    libtool \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
WORKDIR /opt/vcpkg
RUN ./bootstrap-vcpkg.sh

# Enable vcpkg integration
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Copy custom ports
RUN mkdir -p /opt/vcpkg/ports/boost-decimal
COPY vcpkg/ports/boost-decimal/portfile.cmake /opt/vcpkg/ports/boost-decimal/
COPY vcpkg/ports/boost-decimal/vcpkg.json /opt/vcpkg/ports/boost-decimal/

# Copy project files
WORKDIR /app
COPY . .

# Create build directory
RUN mkdir -p build

# Install dependencies using vcpkg
# Use vcpkg from the system path (which we added /opt/vcpkg to)
RUN vcpkg install

# Build the project
WORKDIR /app/build
# Point to the correct toolchain file in /opt/vcpkg
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN make -j$(nproc)

# Run tests
# Tests will be run by the container orchestration, not during build

# Default command
CMD ["./bin/cohida", "--help"]