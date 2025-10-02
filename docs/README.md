# Coinbase Historical Data Retrieval Project

A Python application to retrieve historical cryptocurrency data from Coinbase using the coinbase-advanced-py library, with comprehensive testing and deployment via podman-compose.

## Features

- **Historical Data Retrieval**: Fetch cryptocurrency price data from Coinbase Advanced API
- **PostgreSQL Integration**: Store data with write-as-read capability
- **Command-Line Interface**: Easy-to-use CLI for data operations
- **Comprehensive Testing**: Unit, integration, and end-to-end tests
- **Containerized Deployment**: Docker/podman-compose support
- **Error Handling**: Robust error handling with retry logic
- **Rate Limiting**: Respects API rate limits with exponential backoff

## Project Structure

```
src/
├── coinbase_client.py      # CoinbaseClient class for API authentication
├── data_retriever.py       # HistoricalDataRetriever class for data fetching
├── models.py              # Data models/schemas for cryptocurrency data
├── database.py            # PostgreSQL database operations
├── config.py              # Configuration management
└── cli.py                 # Command-line interface

tests/
├── unit/                  # Unit tests for core functionality
├── integration/           # Integration tests for API and database
└── e2e/                   # End-to-end tests

docs/
└── README.md              # This file

db/
├── postgres-u.txt         # Database username
└── postgres-p.txt         # Database password

outputs/                   # Directory for data output files
```

## Prerequisites

- Python 3.11+
- Docker/Podman
- Coinbase Advanced API credentials
- PostgreSQL database

## Quick Start

### 1. Clone and Setup

```bash
git clone <repository-url>
cd crypto
```

### 2. Configure Environment

Copy the example environment file and configure your API credentials:

```bash
cp env.example .env
```

Edit `.env` with your Coinbase Advanced API credentials:

```env
COINBASE_API_KEY=your_api_key_here
COINBASE_API_SECRET=your_api_secret_here
COINBASE_API_PASSPHRASE=your_passphrase_here
COINBASE_SANDBOX=false
```

### 3. Deploy with Podman Compose

```bash
# Ensure the db_prdnet network exists (should be created by your PostgreSQL setup)
podman network create db_prdnet 2>/dev/null || true

# Start the application (connects to existing PostgreSQL on db_prdnet)
podman-compose up -d

# Check status
podman-compose ps
```

### 4. Test Connections

```bash
# Test API and database connections
podman-compose exec coinbase-app python src/cli.py test
```

### 5. Retrieve Data

```bash
# Retrieve Bitcoin data for the last 7 days
podman-compose exec coinbase-app python src/cli.py retrieve BTC-USD --days 7

# Retrieve Ethereum data for specific date range
podman-compose exec coinbase-app python src/cli.py retrieve ETH-USD --start-date 2023-01-01 --end-date 2023-01-31
```

## Usage

### Command-Line Interface

The application provides a comprehensive CLI with the following commands:

#### Retrieve Historical Data

```bash
# Basic usage - last 7 days
python src/cli.py retrieve BTC-USD

# Specific date range
python src/cli.py retrieve BTC-USD --start-date 2023-01-01 --end-date 2023-01-31

# Custom granularity (1 hour, 1 day, etc.)
python src/cli.py retrieve BTC-USD --granularity 3600

# Output to JSON format
python src/cli.py retrieve BTC-USD --output-format json

# Don't save to database
python src/cli.py retrieve BTC-USD --no-save-to-db
```

#### Read Data from Database

```bash
# Read all data for a symbol
python src/cli.py read BTC-USD

# Read data for specific date range
python src/cli.py read BTC-USD --start-date 2023-01-01 --end-date 2023-01-31
```

#### Test Connections

```bash
# Test API and database connections
python src/cli.py test
```

#### List Available Symbols

```bash
# List all available cryptocurrency symbols
python src/cli.py symbols
```

#### Get Symbol Information

```bash
# Get detailed information about a symbol
python src/cli.py info BTC-USD
```

### Supported Symbols

The application supports major cryptocurrency pairs including:

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

### Data Granularity Options

- `60` - 1 minute
- `300` - 5 minutes
- `900` - 15 minutes
- `3600` - 1 hour (default)
- `21600` - 6 hours
- `86400` - 1 day

## Development

### Local Development Setup

1. **Install Dependencies**

```bash
pip install -r requirements.txt
```

2. **Run Tests**

```bash
# Run all tests
pytest

# Run specific test categories
pytest tests/unit/          # Unit tests only
pytest tests/integration/    # Integration tests only
pytest tests/e2e/           # End-to-end tests only

# Run with coverage
pytest --cov=src --cov-report=html
```

3. **Development Mode**

```bash
# Run CLI directly
python src/cli.py --help

# Run with verbose logging
python src/cli.py --verbose retrieve BTC-USD
```

### Testing

The project includes comprehensive testing:

- **Unit Tests**: Test individual components in isolation
- **Integration Tests**: Test API interactions and database connectivity
- **End-to-End Tests**: Full application workflow testing

Run tests with:

```bash
pytest tests/ -v
```

### Code Quality

The project follows these standards:

- **Architecture**: Modular design with single responsibility principle
- **OOP First**: Every functionality in dedicated classes
- **File Limits**: Maximum 500 lines per file
- **Documentation**: Comprehensive docstrings and comments
- **Type Hints**: Full type annotation coverage

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `COINBASE_API_KEY` | Coinbase Advanced API key | Required |
| `COINBASE_API_SECRET` | Coinbase Advanced API secret | Required |
| `COINBASE_API_PASSPHRASE` | Coinbase Advanced API passphrase | Required |
| `COINBASE_SANDBOX` | Enable sandbox mode | `false` |
| `POSTGRES_HOST` | PostgreSQL host | `postgres` |
| `POSTGRES_PORT` | PostgreSQL port | `5432` |
| `POSTGRES_DB` | PostgreSQL database name | `crypto_data` |
| `LOG_LEVEL` | Logging level | `INFO` |
| `LOG_FORMAT` | Log format (json/console) | `json` |

### Database Configuration

Database credentials are read from files in the `db/` directory:

- `db/postgres-u.txt` - Database username
- `db/postgres-p.txt` - Database password

## API Rate Limits

The application respects Coinbase API rate limits:

- Implements exponential backoff for failed requests
- Retries failed requests up to 3 times
- Handles rate limit responses gracefully

## Error Handling

The application includes comprehensive error handling for:

- **API Errors**: Rate limiting, authentication failures, network issues
- **Database Errors**: Connection failures, query errors, data integrity issues
- **Input Validation**: Symbol formats and date ranges
- **Logging**: Structured logging with appropriate levels

## Output Files

Data is saved to the `outputs/` directory in the following formats:

- **CSV**: Comma-separated values with headers
- **JSON**: Structured JSON format

Files are named with the pattern: `{SYMBOL}_{TIMESTAMP}.{FORMAT}`

Example: `BTC-USD_20231201_143022.csv`

## Troubleshooting

### Common Issues

1. **API Authentication Failed**
   - Verify your API credentials in `.env`
   - Check if you're using sandbox vs production credentials
   - Ensure API key has proper permissions

2. **Database Connection Failed**
   - Verify PostgreSQL container is running on db_prdnet network
   - Check database credentials in `db/` folder
   - Ensure db_prdnet network exists: `podman network ls | grep db_prdnet`
   - Verify database is accessible from application container

3. **No Data Retrieved**
   - Check if symbol is available: `python src/cli.py symbols`
   - Verify date range is valid
   - Check API rate limits

4. **Permission Errors**
   - Ensure output directory is writable
   - Check file permissions in `outputs/` directory

### Debug Mode

Enable verbose logging for debugging:

```bash
python src/cli.py --verbose retrieve BTC-USD
```

### Logs

View application logs:

```bash
# View logs from running container
podman-compose logs coinbase-app

# Follow logs in real-time
podman-compose logs -f coinbase-app
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

For issues and questions:

1. Check the troubleshooting section above
2. Review the test cases for usage examples
3. Check the logs for error details
4. Open an issue on GitHub

## Changelog

### Version 1.0.0
- Initial release
- Coinbase Advanced API integration
- PostgreSQL database support
- Comprehensive CLI interface
- Full test coverage
- Containerized deployment
