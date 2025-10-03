# Coinbase Historical Data Retrieval Project

A Python application to retrieve historical cryptocurrency data from Coinbase using the coinbase-advanced-py library, with comprehensive testing and deployment via podman-compose.

## 🚀 Quick Start

### 1. Configure Environment
```bash
cp env.example .env
# Edit .env with your Coinbase Advanced API credentials
```

### 2. Deploy with Podman Compose
```bash
# Ensure the db_prdnet network exists (should be created by your PostgreSQL setup)
podman network create db_prdnet 2>/dev/null || true

# Start the application (connects to existing PostgreSQL on db_prdnet)
podman-compose up -d
```

### 3. Test Connections
```bash
podman-compose exec coinbase-app python src/cli.py test
```

### 4. Retrieve Data
```bash
# Get Bitcoin data for last 7 days
podman-compose exec coinbase-app python src/cli.py retrieve BTC-USD --days 7
```

## 📋 Features

- ✅ **Historical Data Retrieval** from Coinbase Advanced API
- ✅ **PostgreSQL Integration** with write-as-read capability
- ✅ **Command-Line Interface** for easy data operations
- ✅ **Comprehensive Testing** (unit, integration, e2e)
- ✅ **Containerized Deployment** via podman-compose
- ✅ **Error Handling** with retry logic and rate limiting
- ✅ **Structured Logging** with configurable levels
- ✅ **Multiple Symbol Support** - retrieve data for multiple cryptocurrencies at once
- ✅ **Granularity-Based Table Organization** - separate tables for different time granularities
- ✅ **Optional CSV Output** - save data to files only when requested
- ✅ **Flexible Data Granularity** - support for 1m, 5m, 15m, 1h, 6h, 1d intervals with intuitive shorthand
- ✅ **Auto-Detection of Historical Data** - automatically retrieves ALL available data (10+ years for Bitcoin)
- ✅ **User-Friendly CLI** - intuitive shorthand options and comprehensive error handling

## 🏗️ Project Structure

```
src/
├── coinbase_client.py      # API authentication & connection
├── data_retriever.py       # Historical data fetching
├── models.py              # Data models & schemas
├── database.py            # PostgreSQL operations
├── config.py              # Configuration management
└── cli.py                 # Command-line interface

tests/
├── unit/                  # Unit tests
├── integration/           # Integration tests
└── e2e/                   # End-to-end tests

docs/
└── README.md              # Detailed documentation

db/
├── postgres-u.txt         # Database username
└── postgres-p.txt         # Database password
```

## 🎯 Usage Examples

### Retrieve Historical Data
```bash
# Basic usage - last 7 days
python src/cli.py retrieve BTC-USD

# Multiple symbols at once
python src/cli.py retrieve BTC-USD ETH-USD ADA-USD --days 7

# Specific date range
python src/cli.py retrieve ETH-USD --start-date 2023-01-01 --end-date 2023-01-31

# Custom granularity with shorthand (1 hour)
python src/cli.py retrieve BTC-USD --granularity 1h

# Different granularities
python src/cli.py retrieve BTC-USD --granularity 1d --days 7    # Daily data
python src/cli.py retrieve BTC-USD --granularity 5m --days 1    # 5-minute data
python src/cli.py retrieve BTC-USD --granularity 6h --days 30   # 6-hour data

# Backward compatibility (numeric seconds still work)
python src/cli.py retrieve BTC-USD --granularity 3600

# JSON output format
python src/cli.py retrieve BTC-USD --output-format json

# Save to CSV file (optional)
python src/cli.py retrieve BTC-USD --save-csv

# Retrieve ALL available historical data (auto-detects data boundaries)
python src/cli.py retrieve-all BTC-USD

# Retrieve all data for multiple symbols with auto-detection
python src/cli.py retrieve-all BTC-USD ETH-USD --granularity 1d --save-csv

# Manually limit to specific years (optional)
python src/cli.py retrieve-all BTC-USD --max-years 3 --granularity 1h
```

### Read from Database
```bash
# Read all data for a symbol (default granularity)
python src/cli.py read BTC-USD

# Read specific date range with granularity
python src/cli.py read BTC-USD --start-date 2023-01-01 --end-date 2023-01-31 --granularity 1h

# Read different granularity data
python src/cli.py read BTC-USD --granularity 1d    # Daily data
python src/cli.py read BTC-USD --granularity 5m    # 5-minute data
python src/cli.py read BTC-USD --granularity 6h    # 6-hour data
```

### Utility Commands
```bash
# Test connections
python src/cli.py test

# List available symbols
python src/cli.py symbols

# Get symbol information
python src/cli.py info BTC-USD
```

## 🔧 Development

### Run Tests
```bash
# All tests
pytest

# Specific categories
pytest tests/unit/          # Unit tests
pytest tests/integration/   # Integration tests
pytest tests/e2e/          # End-to-end tests

# With coverage
pytest --cov=src --cov-report=html
```

### Local Development
```bash
pip install -r requirements.txt
python src/cli.py --help
```

## 📊 Data Availability

### Historical Data Coverage
- **Bitcoin (BTC-USD)**: July 2015 - Present (10+ years)
- **Ethereum (ETH-USD)**: Available from launch date
- **Other Major Cryptocurrencies**: Varies by launch date
- **Auto-Detection**: Automatically finds the earliest available data for each symbol

### Supported Symbols
All major cryptocurrencies available on Coinbase Advanced API, including:
- BTC-USD (Bitcoin) - 10+ years of data
- ETH-USD (Ethereum)
- ADA-USD (Cardano)
- DOT-USD (Polkadot)
- LINK-USD (Chainlink)
- UNI-USD (Uniswap)
- LTC-USD (Litecoin)
- BCH-USD (Bitcoin Cash)
- XLM-USD (Stellar)
- FIL-USD (Filecoin)

*Use `python src/cli.py symbols` to see all available symbols*

## ⚙️ Configuration

### Environment Variables
| Variable | Description | Default |
|----------|-------------|---------|
| `COINBASE_API_KEY` | Coinbase Advanced API key | Required |
| `COINBASE_API_SECRET` | Coinbase Advanced API secret | Required |
| `COINBASE_API_PASSPHRASE` | Coinbase Advanced API passphrase | Required |
| `COINBASE_SANDBOX` | Enable sandbox mode | `false` |
| `LOG_LEVEL` | Logging level | `INFO` |

### Data Granularity Options

**Shorthand (Recommended):**
- `1m` - 1 minute
- `5m` - 5 minutes  
- `15m` - 15 minutes
- `1h` - 1 hour (default)
- `6h` - 6 hours
- `1d` - 1 day

**Numeric (Backward Compatible):**
- `60` - 1 minute
- `300` - 5 minutes
- `900` - 15 minutes
- `3600` - 1 hour
- `21600` - 6 hours
- `86400` - 1 day

## 📁 Output Files

Data is saved to `outputs/` directory in CSV or JSON format (only when `--save-csv` flag is used):
- **CSV**: `BTC-USD_20231201_143022.csv`
- **JSON**: `BTC-USD_20231201_143022.json`
- **Complete Historical**: `BTC-USD_ALL_20231201_143022.csv`

### Example Data Volumes
- **Bitcoin Daily Data**: ~3,700 data points (July 2015 - Present)
- **Bitcoin Hourly Data**: ~87,500 data points (October 2015 - Present)
- **Auto-Detection**: Retrieves maximum available data for each symbol

## 🗄️ Database Organization

Data is organized by granularity in separate tables within the same database:
- **crypto_1m** - 1-minute granularity data
- **crypto_5m** - 5-minute granularity data  
- **crypto_15m** - 15-minute granularity data
- **crypto_1h** - 1-hour granularity data (default)
- **crypto_6h** - 6-hour granularity data
- **crypto_1d** - 1-day granularity data

## 🔍 Troubleshooting

### Common Issues
1. **API Authentication Failed**: Verify credentials in `.env`
2. **Database Connection Failed**: Check PostgreSQL container status
3. **No Data Retrieved**: Verify symbol availability and date range

### Debug Mode
```bash
python src/cli.py --verbose retrieve BTC-USD
```

### View Logs
```bash
podman-compose logs coinbase-app
```

## 📚 Documentation

For detailed setup instructions, API documentation, and advanced usage, see:
- [Complete Documentation](docs/README.md)
- [Project Specification](spec.md)
- [Development Tasks](TODO.md)

## ✅ Project Status

**All 30 development tasks completed successfully!**

The project is production-ready with:
- Complete implementation of all core features
- Advanced features including multiple symbol support
- Granularity-based table organization
- Optional CSV output functionality
- **Auto-detection of all available historical data (10+ years)**
- **User-friendly CLI with intuitive shorthand options**
- Comprehensive test coverage
- Full documentation
- Containerized deployment
- Error handling and logging

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## 📄 License

This project is licensed under the MIT License.
