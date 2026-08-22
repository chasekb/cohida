# cohida: Coinbase Historical Data Retrieval (C++ Version)

A high-performance C++ implementation for retrieving, storing, and analyzing historical cryptocurrency data from Coinbase Advanced API.

## 🚀 Features

- **Blazing Fast**: C++20 implementation for maximum efficiency and performance.
- **Optional ML Support**: XGBoost and LightGBM training/inference when the optional libraries are available at build time.
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
cd cpp-cohida
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
make -j2
```

## 💻 Usage Examples

### 1. Configure Environment

Create a `.env` file based on `env.example`. The C++ loader gives precedence to
the `POSTGRES_DB_*` variables shown below, then accepts corresponding `DB_*`
names as fallbacks:

```env
COINBASE_API_KEY=your_key
COINBASE_API_SECRET=your_secret
COINBASE_API_PASSPHRASE=your_passphrase
POSTGRES_DB_HOST=localhost
POSTGRES_DB_PORT=5432
POSTGRES_DB_NAME=cohida
POSTGRES_DB_USER=postgres
POSTGRES_DB_PASSWORD=postgres
POSTGRES_DB_SCHEMA=public
POSTGRES_DB_TABLE=crypto
...
```

### 2. Basic Commands

```bash
# Test connections
./cpp-cohida/build/bin/cohida test

# List available symbols
./cpp-cohida/build/bin/cohida symbols

# Retrieve Bitcoin data for a date range
./cpp-cohida/build/bin/cohida retrieve -s BTC-USD --start 2024-01-01 --end 2024-02-01 -g 3600

# Train an ML model
./cpp-cohida/build/bin/cohida ml-train -s BTC-USD --model-type xgboost

# Advanced: Pipe symbols into retrieve-all (to fetch data for all symbols)
./cpp-cohida/build/bin/cohida symbols --list | xargs -I {} ./cpp-cohida/build/bin/cohida retrieve-all -s {}
```

## 🐳 Deployment

### Local Development

Build the C++ application from source against the named production database
network. Start the database first, then build/run the application compose file:

```bash
podman-compose -f podman-compose.prod.yml up -d db
cd cpp-cohida
podman-compose up --build
```

### Production (Pre-built Images)

Pull and run the latest production-ready image from GitHub Container Registry (GHCR):

```bash
# Start the production database and application in the background. The app has
# an explicit healthy-database dependency, so ordinary up/run commands do not
# launch application work before PostgreSQL is ready.
podman-compose -f podman-compose.prod.yml up -d
```

### Production Usage Examples

When running one-off production jobs, the supplied scripts start PostgreSQL,
wait for its healthcheck, verify DNS, and test application connectivity before
running any application command.

The production compose file creates the named `cohida-net` network and
advertises its database service as `cohida-db`. The C++ and legacy compose files
attach to that same named network and use the same database alias. Production
explicitly overrides both `DB_HOST` and `POSTGRES_DB_HOST` to `cohida-db` so
stale `.env` host values cannot select another network.

The guarded entrypoint is the sole recommended production retrieval path. It
waits up to five minutes for `cohida-db-prod`, verifies `cohida-db` DNS from
the application network, tests connectivity, and stops before retrieval if any
preflight fails:

```bash
# Fetch all symbols across the supported granularities.
./scripts/retrieve-production.sh

# Or fetch selected granularities after the same preflight.
./scripts/retrieve-production.sh 3600 86400
```

For a one-off production application command, use the database-starting runner;
it performs the same preflight before passing arguments to the app container:

```bash
./scripts/run-production.sh ./bin/cohida test
```

Do not invoke an unguarded raw loop for production retrieval; use
`scripts/retrieve-production.sh` so PostgreSQL is started and verified before
the loop. Its application runs intentionally omit `--no-deps`, allowing the
Compose `db` dependency to be honored as an additional startup guard.

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
./cpp-cohida/build/bin/cohida-unit-tests
```

## 📚 Documentation

- [Migration Guide](docs/migration_guide.md)
- [Current GitHub Actions workflow](.github/workflows/deploy.yml)
- [C++ Conversion TODO List](docs/cpp_todo.md)

## 📄 License

MIT License
