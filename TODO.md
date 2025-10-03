# Coinbase Historical Data Retrieval Project - TODO

## Project Overview
Python application to retrieve historical cryptocurrency data from Coinbase using coinbase-advanced-py library, with testing and deployment via podman-compose.

## Development Tasks

### Phase 1: Project Foundation
- [x] Initialize Python project structure with proper directories (src/, tests/, docs/)
- [x] Create requirements.txt with coinbase-advanced-py, psycopg2, python-dotenv, and other dependencies
- [x] Create Dockerfile and docker-compose.yml for podman-compose deployment
- [x] Initialize git repository and create .gitignore
- [x] Create and maintain local git repository with regular commits and descriptive messages
- [x] Create database connection management and secrets handling from db folder
- [x] Create database configuration file for database, schema, and table names (avoid hardcoding)

### Phase 2: Core Implementation
- [x] Implement CoinbaseClient class for API authentication and connection
- [x] Create HistoricalDataRetriever class to fetch historical data for given symbols
- [x] Define data models/schemas for cryptocurrency data structures
- [x] Implement PostgreSQL database output functionality with write-as-read capability
- [x] Create command-line interface for symbol input and data retrieval

### Phase 3: Robustness & Quality
- [x] Implement comprehensive error handling and logging
- [x] Configure structured logging with appropriate levels
- [x] Implement rate limiting and retry logic for API calls
- [x] Create configuration management for API keys and settings from .env file
- [x] Add input validation for symbol format and date ranges

### Phase 4: Testing
- [x] Write unit tests for all core functionality
- [x] Create integration tests for Coinbase API interactions
- [x] Create database integration tests for PostgreSQL connectivity and data persistence
- [x] Test full application deployment and functionality with podman-compose
- [x] End-to-end testing with real Coinbase data retrieval and PostgreSQL storage

### Phase 5: Documentation & Finalization
- [x] Write README.md with setup and usage instructions
- [x] Document all sources used and data retrieval methods
- [x] Final validation and system-wide consistency check

## Progress Tracking
- **Total Tasks**: 28
- **Completed**: 28
- **In Progress**: 0
- **Remaining**: 0

### Phase 6: Enhanced Features
- [x] Provide option to retrieve all historical data for a symbol

### Phase 7: Advanced Features & Optimizations
- [x] ~~Create separate databases for different granularities~~ (CANCELLED - using separate tables instead)
- [x] Append granularity to table name (e.g., crypto_1m for 1 minute granularity)
- [x] Create ability to pass multiple symbols at once
- [x] Produce CSV output only when requested
- [x] Create separate tables for different granularities in default database
- [x] Update all code to use appended table names in default database

## Project Status: ✅ FULLY COMPLETED
All development tasks and advanced features have been successfully completed. The Coinbase Historical Data Retrieval project is fully functional with granularity-based table separation, multiple symbol support, and optional CSV output.

## Notes
- Follow architectural principles: modular design, single responsibility, OOP-first approach
- Maintain file length limits (max 500 lines per file)
- Use proper error handling and logging throughout
- Ensure all data is retrieved from actual Coinbase API (no synthetic data)
- PostgreSQL database already exists in separate container
- Database secrets are stored in db/ folder
- Coinbase authentication credentials are located in .env file
- Maintain git repository with regular commits and descriptive messages throughout development
- Implement human-like browsing behavior for web interactions if needed
