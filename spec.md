# Coinbase Historical Data Retrieval Project - Specification

## Project Overview
A Python application to retrieve historical cryptocurrency data from Coinbase using the coinbase-advanced-py library, with comprehensive testing and deployment via podman-compose.

## Core Requirements

### Functional Requirements
- **Data Source**: Retrieve historical cryptocurrency data from Coinbase API using coinbase-advanced-py library
- **Data Storage**: Store retrieved data in PostgreSQL database with write-as-read capability
- **Input Interface**: Command-line interface for symbol input and data retrieval
- **Authentication**: Secure API key management via .env file
- **Database Integration**: Connect to existing PostgreSQL container using credentials from db/ folder

### Technical Requirements
- **Language**: Python 3.x
- **Containerization**: Docker/podman-compose deployment
- **Database**: PostgreSQL (existing container)
- **API Library**: coinbase-advanced-py
- **Dependencies**: psycopg2, python-dotenv, logging, testing frameworks

## Architecture Design

### Project Structure
```
src/
├── coinbase_client.py      # CoinbaseClient class for API authentication
├── data_retriever.py      # HistoricalDataRetriever class for data fetching
├── models.py              # Data models/schemas for cryptocurrency data
├── database.py            # PostgreSQL database operations
├── config.py              # Configuration management
└── cli.py                 # Command-line interface

tests/
├── unit/                  # Unit tests for core functionality
├── integration/           # Integration tests for API and database
└── e2e/                   # End-to-end tests

docs/
└── README.md              # Setup and usage instructions

db/
├── postgres-u.txt         # Database username
└── postgres-p.txt         # Database password

.env                       # API keys and environment variables
requirements.txt           # Python dependencies
Dockerfile                 # Container configuration
docker-compose.yml         # Podman-compose deployment
.gitignore                 # Git ignore patterns
```

### Core Components

#### 1. CoinbaseClient Class
- **Purpose**: Handle API authentication and connection to Coinbase
- **Responsibilities**:
  - Initialize API client with credentials
  - Manage authentication tokens
  - Handle API rate limiting
  - Provide connection status validation

#### 2. HistoricalDataRetriever Class
- **Purpose**: Fetch historical data for given cryptocurrency symbols
- **Responsibilities**:
  - Accept symbol input and date range parameters
  - Retrieve historical price data from Coinbase API
  - Implement retry logic for failed requests
  - Validate and transform data before storage

#### 3. Data Models
- **Purpose**: Define structured data schemas for cryptocurrency data
- **Components**:
  - Price data structure (timestamp, open, high, low, close, volume)
  - Symbol validation schema
  - Database table mapping

#### 4. Database Operations
- **Purpose**: PostgreSQL integration with write-as-read capability
- **Responsibilities**:
  - Establish database connections using db/ folder credentials
  - Create/update database schema and tables
  - Implement write-as-read data persistence
  - Handle database connection pooling and error recovery

#### 5. Configuration Management
- **Purpose**: Centralized configuration for API keys, database settings, and application parameters
- **Responsibilities**:
  - Load environment variables from .env file
  - Manage database configuration (host, port, database name, schema)
  - Configure logging levels and output formats
  - Handle sensitive credential management

#### 6. Command-Line Interface
- **Purpose**: User interface for symbol input and data retrieval operations
- **Features**:
  - Accept cryptocurrency symbol as input parameter
  - Support date range specification
  - Display progress and status information
  - Handle user input validation

## Quality Assurance

### Testing Strategy
- **Unit Tests**: Test individual components in isolation
- **Integration Tests**: Test API interactions and database connectivity
- **End-to-End Tests**: Full application workflow testing with real data
- **Database Tests**: PostgreSQL connectivity and data persistence validation

### Error Handling
- **API Errors**: Handle rate limiting, authentication failures, and network issues
- **Database Errors**: Manage connection failures, query errors, and data integrity issues
- **Input Validation**: Validate symbol formats and date ranges
- **Logging**: Structured logging with appropriate levels for debugging and monitoring

### Performance Requirements
- **Rate Limiting**: Respect Coinbase API rate limits
- **Retry Logic**: Implement exponential backoff for failed requests
- **Connection Pooling**: Efficient database connection management
- **Memory Management**: Handle large datasets without memory issues

## Deployment Specifications

### Container Configuration
- **Base Image**: Python 3.x official image
- **Dependencies**: Install from requirements.txt
- **Environment**: Configure via .env file and docker-compose.yml
- **Networking**: Connect to existing PostgreSQL container

### Database Configuration
- **Connection**: Use credentials from db/postgres-u.txt and db/postgres-p.txt
- **Schema**: Configurable database and table names (no hardcoding)
- **Persistence**: Data stored in PostgreSQL with proper indexing

## Development Standards

### Code Quality
- **Architecture**: Modular design with single responsibility principle
- **OOP First**: Every functionality in dedicated classes/structs
- **File Limits**: Maximum 500 lines per file
- **Documentation**: Comprehensive docstrings and comments

### Git Management
- **Repository**: Maintain local git repository with regular commits
- **Commit Messages**: Descriptive messages indicating changes made
- **Branching**: Feature-based development workflow

### Security
- **Credentials**: Secure handling of API keys and database passwords
- **Environment**: Use .env files for sensitive configuration
- **Validation**: Input sanitization and validation throughout

## Success Criteria
- Successfully retrieve historical data from Coinbase API
- Store data in PostgreSQL database with write-as-read capability
- Pass all unit, integration, and end-to-end tests
- Deploy successfully using podman-compose
- Maintain clean, modular, and well-documented codebase
- Handle errors gracefully with comprehensive logging