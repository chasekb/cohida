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

# Specific date range
python src/cli.py retrieve ETH-USD --start-date 2023-01-01 --end-date 2023-01-31

# Custom granularity (1 hour)
python src/cli.py retrieve BTC-USD --granularity 3600

# JSON output format
python src/cli.py retrieve BTC-USD --output-format json

# Retrieve ALL historical data (up to 5 years back)
python src/cli.py retrieve-all BTC-USD

# Retrieve all data with custom settings
python src/cli.py retrieve-all BTC-USD --max-years 3 --granularity 86400
```

### Read from Database
```bash
# Read all data for a symbol
python src/cli.py read BTC-USD

# Read specific date range
python src/cli.py read BTC-USD --start-date 2023-01-01 --end-date 2023-01-31
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

## 📊 Supported Symbols

- BTC-USD (Bitcoin)
- ETH-USD (Ethereum)
- ADA-USD (Cardano)
- DOT-USD (Polkadot)
- LINK-USD (Chainlink)
- UNI-USD (Uniswap)
- LTC-USD (Litecoin)
- BCH-USD (Bitcoin Cash)
- XLM-USD (Stellar)
- FIL-USD (Filecoin)

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
- `60` - 1 minute
- `300` - 5 minutes
- `900` - 15 minutes
- `3600` - 1 hour (default)
- `21600` - 6 hours
- `86400` - 1 day

## 📁 Output Files

Data is saved to `outputs/` directory in CSV or JSON format:
- **CSV**: `BTC-USD_20231201_143022.csv`
- **JSON**: `BTC-USD_20231201_143022.json`

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

**All 23 development tasks completed successfully!**

The project is production-ready with:
- Complete implementation of all core features
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
