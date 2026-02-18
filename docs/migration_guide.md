# Migration Guide: Python to C++

This document assists users in transitioning from the legacy Python implementation to the high-performance C++ version of the cohida project.

## 🚀 Overview

The C++ version provides a significant performance increase, lower memory footprint, and improved type safety. While the core functionality remains the same, there are some differences in the CLI and configuration.

## 🔧 Configuration

### Environment Variables

| Python Variable | C++ Variable | Notes |
|-----------------|--------------|-------|
| `COINBASE_API_KEY` | `COINBASE_API_KEY` | Same |
| `COINBASE_API_SECRET` | `COINBASE_API_SECRET` | Same |
| `COINBASE_API_PASSPHRASE` | `COINBASE_API_PASSPHRASE` | Same |
| `POSTGRES_DB` | `POSTGRES_DB` | Same |
| `POSTGRES_USER` | `POSTGRES_USER` | C++ supports reading from `POSTGRES_USER_FILE` |
| `POSTGRES_PASSWORD` | `POSTGRES_PASSWORD` | C++ supports reading from `POSTGRES_PASSWORD_FILE` |

## 💻 Command-Line Interface

The C++ CLI uses `CLI11` and follows a similar but stricter subcommand structure.

### Common Commands

| Purpose | Python Command | C++ Command |
|---------|----------------|-------------|
| Test Connection | `python src/cli.py test` | `./bin/cohida test` |
| List Symbols | `python src/cli.py symbols` | `./bin/cohida symbols` |
| Get Symbol Info | `python src/cli.py info BTC-USD` | `./bin/cohida info -s BTC-USD` |
| Retrieve Data | `python src/cli.py retrieve BTC-USD` | `./bin/cohida retrieve -s BTC-USD --start 2024-01-01 --end 2024-01-02` |
| Retrieve All | `python src/cli.py retrieve-all BTC-USD` | `./bin/cohida retrieve-all -s BTC-USD` |
| Read from DB | `python src/cli.py read BTC-USD` | `./bin/cohida read -s BTC-USD --start 2024-01-01 --end 2024-01-02` |

### Machine Learning Commands

C++ provides native ML support for XGBoost and LightGBM.

| Purpose | Python Command | C++ Command |
|---------|----------------|-------------|
| Train Model | N/A (Mostly internal) | `./bin/cohida ml-train -s BTC-USD` |
| List Models | N/A | `./bin/cohida ml-models` |
| Model Info | N/A | `./bin/cohida ml-info --model-id <ID>` |

### 🛠️ Advanced Piping (New in C++)

The C++ version supports clean piping by sending log messages to `stderr`. You can now easily automate tasks using standard shell tools.

**Example: Automated data retrieval for all symbols**

```bash
./bin/cohida symbols --list | xargs -I {} ./bin/cohida retrieve-all -s {}
```

## 📦 Deployment

### Containerization

- **Python**: Uses `Dockerfile` with `pip install`.
- **C++**: Uses a multi-stage `Dockerfile` with `vcpkg` for dependency management.
- **Compose**: `podman-compose` or `docker-compose` can still be used.

### Build System

The C++ project uses CMake. You must install dependencies via `vcpkg` before building.

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
make -j2
```

## 📊 Performance Comparison

- **Memory Usage**: Python (~100-200MB) vs C++ (~20-50MB) for typical retrieval runs.
- **Execution Speed**: C++ is approximately 5-10x faster for intensive technical indicator calculations.
- **Binary Size**: The C++ application is a single executable (plus dependencies) and starts up instantly.
