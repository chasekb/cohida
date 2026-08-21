# cohida: Coinbase Historical Data Retrieval (C++ Version)

A high-performance C++ implementation for retrieving, storing, and analyzing historical cryptocurrency data from Coinbase Advanced API.

## 🚀 Features

- **Blazing Fast**: C++20 implementation for maximum efficiency and performance.
- **Native ML Support**: Integrated XGBoost and LightGBM training and inference.
- **High-Performance Technical Indicators**: Internal indicator library optimized for speed.
- **Robust Database Integration**: PostgreSQL support with connection pooling and upsert capability.
- **Container Ready**: Multi-stage Docker build for easy deployment.
- **Scalable**: Handles massive datasets with chunked retrieval and optimized storage.

## 📋 Prerequisites

- C++20 Compatible Compiler (GCC 11+, Clang 13+, MSVC 2019+)
- CMake 3.20+
- [vcpkg](https://github.com/microsoft/vcpkg) for dependency management
- PostgreSQL 12+

## 🛠️ Build Instructions

### 1. Install Dependencies

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=$(pwd)/vcpkg
```

### 2. Build the Project

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
make -j2
```

## 💻 Usage Examples

### 1. Configure Environment

Create a `.env` file based on `env.example`:

```env
COINBASE_API_KEY=your_key
COINBASE_API_SECRET=your_secret
COINBASE_API_PASSPHRASE=your_passphrase
POSTGRES_HOST=localhost
POSTGRES_DB=cohida
...
```

### 2. Basic Commands

```bash
# Test connections
./bin/cohida test

# List available symbols
./bin/cohida symbols

# Retrieve Bitcoin data for a date range
./bin/cohida retrieve -s BTC-USD --start 2024-01-01 --end 2024-02-01 -g 3600

# Train an ML model
./bin/cohida ml-train -s BTC-USD --model-type xgboost

# Advanced: Pipe symbols into retrieve-all (to fetch data for all symbols)
./bin/cohida symbols --list | xargs -I {} ./bin/cohida retrieve-all -s {}
```

## 🐳 Deployment

### Local Development

Build and run everything locally from source:

```bash
podman-compose up --build
```

### Production (Pre-built Images)

Pull and run the latest production-ready image from GitHub Container Registry (GHCR):

```bash
# Start the database and application in the background
podman-compose -f podman-compose.prod.yml up -d
```

### Production Usage Examples

When running one-off production jobs, start PostgreSQL once and wait for its
healthcheck before using `run --no-deps`. This prevents each loop iteration
from recreating PostgreSQL and racing its crash recovery.

The production compose file provides the `db` DNS alias only on
`cohida-net`. The C++ development/test and legacy Python compose files join
the external `db_prdnet` network, where the standalone PostgreSQL service is
advertised as `postgres`; the C++ files explicitly override `DB_HOST` and
`DB_PORT`, while the legacy Python file sets `POSTGRES_HOST` and
`POSTGRES_PORT`. Do not mix the two compose projects or run an
application container on one network while its `DB_HOST` points at the other
project's service name. If a container on `db_prdnet` reports `could not
translate host name "db"`, inspect the rendered compose environment first:
the container is using the production alias against the shared network.

```bash
set -e

# Start PostgreSQL once and wait until it accepts connections
podman-compose -f podman-compose.prod.yml up -d db
until [ "$(podman inspect cohida-db-prod --format '{{.State.Health.Status}}')" = healthy ]; do
    sleep 5
done

# Optional DNS sanity check from the same compose network
podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app getent hosts db

# Do not start a retrieval loop if the application cannot resolve its DB alias.
# Production compose explicitly injects DB_HOST=db and DB_PORT=5432 after .env.

# Test connections
podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app ./bin/cohida test

# List available symbols
podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app ./bin/cohida symbols

# Retrieve Bitcoin data for a date range
podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app ./bin/cohida retrieve -s BTC-USD --start 2024-01-01 --end 2024-02-01 -g 3600

# Advanced: Fetch data for all symbols for a single granularity (e.g., 1 hour)
podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app sh -c './bin/cohida symbols --list | xargs -I {} ./bin/cohida retrieve-all -s {} -g 3600'

# Advanced: Fetch data for all symbols across multiple granularities (e.g., 1h and 1d)
for g in 3600 86400; do
    podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app sh -c "./bin/cohida symbols --list | xargs -I {} ./bin/cohida retrieve-all -s {} -g $g"
done
```

### Supported Granularities

The following granularities (in seconds) are supported by the Coinbase Advanced Trade API and the `cohida` CLI:

| Seconds | Duration   | Name (Coinbase API) |
|---------|------------|---------------------|
| 60      | 1 minute   | `ONE_MINUTE`        |
| 300     | 5 minutes  | `FIVE_MINUTE`       |
| 900     | 15 minutes | `FIFTEEN_MINUTE`    |
| 3600    | 1 hour     | `ONE_HOUR`          |
| 21600   | 6 hours    | `SIX_HOUR`          |
| 86400   | 1 day      | `ONE_DAY`           |

## 🧪 Testing

Run the comprehensive unit test suite:

```bash
./bin/cohida-unit-tests
```

## 📚 Documentation

- [Migration Guide](docs/migration_guide.md)
- [Recommended GitHub Actions](docs/recommended_github_actions.md)
- [C++ Conversion TODO List](docs/cpp_todo.md)

## 📄 License

MIT License
