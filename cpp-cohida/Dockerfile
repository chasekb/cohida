# Build stage
FROM ubuntu:22.04 AS builder

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
COPY custom_ports/boost-decimal/portfile.cmake /opt/vcpkg/ports/boost-decimal/
COPY custom_ports/boost-decimal/vcpkg.json /opt/vcpkg/ports/boost-decimal/

# Copy project files
WORKDIR /app
COPY . .

# Create build directory
RUN mkdir -p build

# Install dependencies using vcpkg
RUN vcpkg install

# Build the project
WORKDIR /app/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
RUN make -j2

# Runtime stage
FROM ubuntu:22.04

WORKDIR /app

# Install runtime dependencies (libpq is needed for libpqxx)
RUN apt-get update && apt-get install -y \
    libpq5 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy binaries from builder
COPY --from=builder /app/build/bin/cohida ./bin/cohida
COPY --from=builder /app/build/bin/cohida-unit-tests ./bin/cohida-unit-tests

# Copy .env example and instructions (optional, since we mount .env)
COPY .env.example .env.example

# Create output directory
RUN mkdir -p outputs

# Default command
CMD ["./bin/cohida", "--help"]