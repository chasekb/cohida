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
# The production application uses the shared PostgreSQL service on db_prdnet.
podman network inspect db_prdnet >/dev/null
podman-compose -f podman-compose.prod.yml up -d
```

### Production Usage Examples

When running one-off production jobs, verify the shared PostgreSQL service is
healthy before using `run --no-deps`. This prevents each loop iteration from
starting against a missing database or racing database recovery.

The production, C++ development/test, and legacy Python compose files use the
external `db_prdnet` network. The standalone PostgreSQL service is advertised
as `postgres`; production explicitly overrides both `DB_HOST` and
`POSTGRES_DB_HOST` to that alias. Do not use the retired `db` hostname for
production retrieval. The database credentials supplied through the local
`.env` must match the existing shared PostgreSQL service; the preflight stops
before retrieval when authentication fails.

The guarded entrypoint is the sole recommended production retrieval path. It
waits up to five minutes for the shared PostgreSQL service, verifies `postgres`
DNS from
the application network, tests connectivity, and stops before retrieval if any
preflight fails:

```bash
# Fetch all symbols across the supported granularities.
./scripts/retrieve-production.sh

# Or fetch selected granularities after the same preflight.
./scripts/retrieve-production.sh 3600 86400
```

Use the raw `podman-compose run` commands only for diagnostics after the
entrypoint has completed its preflight. Do not use a raw loop for production
retrieval; it can repeatedly launch containers against a missing or detached
database and hide the first actionable failure.

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
