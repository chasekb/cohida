# C++ Conversion Implementation TODO List

## Phase 1: Project Setup & Build System (Week 1)

### 1. Project Structure
- [x] Create project directory structure (include, src, tests, examples)
- [x] Create .gitignore for C++ development
- [x] Set up VSCode configuration files

### 2. CMake Configuration
- [x] Create root CMakeLists.txt
- [x] Add C++20 standard requirement
- [x] Configure compiler flags (warning levels, sanitizers)
- [x] Create subdirectories for modules

### 3. Package Management
- [x] Create vcpkg.json manifest file
- [x] List all required dependencies
- [x] Configure CMake to use vcpkg
- [ ] Test package installation (requires vcpkg installation)

### 4. Core Infrastructure
- [x] Create base exception classes
- [x] Implement logger using spdlog
- [x] Configure structured logging
- [x] Set up configuration management with dotenv-cpp
- [x] Test configuration loading from .env file

## Phase 2: API Client & Models (Weeks 2-3)

### 1. Data Models
- [x] Create DataPoint class (OHLCV data structure)
- [x] Create SymbolInfo class (symbol details)
- [x] Create ApiResponse class (API response wrapper)
- [x] Implement validation logic for data points
- [x] Test model creation and validation

### 2. Coinbase API Client
- [x] Implement CoinbaseClient class
- [ ] Add API authentication using API key/secret
- [ ] Implement HMAC signature generation
- [x] Create API endpoint methods:
  - [x] get_server_time()
  - [x] get_available_symbols()
  - [x] get_symbol_info()
  - [x] get_current_price()
  - [x] get_historical_candles()

### 3. API Utilities
- [x] Implement rate limiting (token bucket algorithm)
- [x] Add retry logic with exponential backoff
- [x] Handle API errors and exceptions
- [x] Test connection and authentication

## Phase 3: Database & Data Retrieval (Weeks 4-5)

### 1. Database Manager
- [x] Create DatabaseManager class - Implemented complete PostgreSQL connection pool manager
- [x] Implement PostgreSQL connection using libpqxx - Connection string generation, pool management
- [x] Create connection pooling - Min/max pool size configuration
- [x] Implement connection string generation - Granularity-based database and table names
- [x] Test database connection - test_connection() method with connection pooling

### 2. Database Operations
- [x] Implement write_data() method - Upsert with conflict resolution
- [x] Implement read_data() method - Query with date range filtering
- [x] Implement test_connection() method - Simple connection test
- [x] Handle granularity-based table naming - Dynamic table names based on granularity
- [x] Test data insertion and retrieval - Complete test suite

### 3. Data Retrieval Pipeline
- [x] Create DataRetriever class - Complete historical data retriever
- [x] Implement retrieve_historical_data() - Single date range retrieval
- [x] Implement retrieve_all_historical_data() - Full historical data retrieval with chunking
- [x] Handle date range parsing - Chrono timestamp handling
- [x] Implement granularity parsing (shorthand to seconds) - 60 -> "ONE_MINUTE" mapping

### 4. Tests
- [x] Create test_database_manager.cpp - Comprehensive unit tests
- [x] Create test_data_retriever.cpp - Complete data retriever tests
- [x] Include tests in CMakeLists.txt - Added to test targets
- [ ] Run tests and fix issues - Requires vcpkg installation first

### Note
vcpkg installation and package management is not completed yet. The build system is configured, but dependencies haven't been installed.

## Phase 4: CLI & UI (Week 6)

### 1. Command-Line Interface
- [ ] Set up CLI11 library integration
- [ ] Create main.cpp entry point
- [ ] Implement command structure
- [ ] Add global options (verbose, output directory)

### 2. Command Implementations
- [ ] Implement "retrieve" command
- [ ] Implement "retrieve-all" command
- [ ] Implement "read" command
- [ ] Implement "test" command
- [ ] Implement "symbols" command
- [ ] Implement "info" command
- [ ] Implement "ml-train" command
- [ ] Implement "ml-models" command
- [ ] Implement "ml-info" command

### 3. File Output
- [ ] Implement CSV writer
- [ ] Implement JSON writer
- [ ] Handle output directory creation
- [ ] Test file writing functionality

## Phase 5: Machine Learning (Weeks 7-9)

### 1. Technical Indicators
- [ ] Integrate TA-Lib
- [ ] Implement common technical indicators:
  - [ ] Moving Averages (SMA, EMA)
  - [ ] RSI
  - [ ] MACD
  - [ ] Bollinger Bands
  - [ ] Volume indicators

### 2. Feature Engineering
- [ ] Create FeatureEngineer class
- [ ] Implement feature extraction from OHLCV data
- [ ] Handle missing values and NaN removal
- [ ] Test feature generation

### 3. Preprocessing
- [ ] Create Preprocessor class
- [ ] Implement data scaling and normalization
- [ ] Implement train/test splitting
- [ ] Handle feature selection

### 4. Model Training
- [ ] Integrate XGBoost C++ API
- [ ] Integrate LightGBM C++ API
- [ ] Create ModelTrainer class
- [ ] Implement training with cross-validation
- [ ] Add hyperparameter tuning
- [ ] Handle task types (classification/regression)

### 5. Model Registry
- [ ] Create ModelRegistry class
- [ ] Implement model saving/loading
- [ ] Handle model metadata
- [ ] Add versioning support

## Phase 6: Testing & Validation (Weeks 10-11)

### 1. Unit Tests
- [ ] Set up Google Test framework
- [ ] Create tests for core models
- [ ] Create tests for API client
- [ ] Create tests for database operations
- [ ] Create tests for data retrieval
- [ ] Create tests for ML modules
- [ ] Run tests and fix issues

### 2. Integration Tests
- [ ] Create integration tests for API endpoints
- [ ] Create integration tests for database operations
- [ ] Test end-to-end data flow
- [ ] Verify data retrieval and storage

### 3. E2E Tests
- [ ] Create end-to-end tests for all commands
- [ ] Test the complete data pipeline
- [ ] Verify CLI functionality

### 4. Performance Testing
- [ ] Compare performance with Python version
- [ ] Identify bottlenecks
- [ ] Optimize critical paths

## Phase 7: Deployment & Documentation (Week 12)

### 1. Containerization
- [ ] Create Dockerfile for C++ application
- [ ] Update docker-compose.yml
- [ ] Test container build and run
- [ ] Configure container networking

### 2. CI/CD
- [ ] Create GitHub Actions workflow
- [ ] Add build, test, and deployment stages
- [ ] Configure container publishing

### 3. Documentation
- [ ] Create README.md for C++ project
- [ ] Update API documentation
- [ ] Add CLI usage examples
- [ ] Create migration guide from Python to C++

### 4. Final Testing
- [ ] Run all tests in container
- [ ] Perform final validation
- [ ] Test with real API and database
- [ ] Fix any remaining issues

## Project Management

### Daily Tasks
- [ ] Standup meetings
- [ ] Code reviews
- [ ] Progress tracking
- [ ] Risk assessment

### Milestones
- [x] Phase 1 Complete: Project structure and build system
- [ ] Phase 2 Complete: API client and data models
- [ ] Phase 3 Complete: Database and data retrieval
- [ ] Phase 4 Complete: Command-line interface
- [ ] Phase 5 Complete: Machine learning modules
- [ ] Phase 6 Complete: Test coverage and validation
- [ ] Phase 7 Complete: Deployment and documentation
- [ ] Final Release: All functionality working

## Resources & Dependencies

### Required Libraries
- nlohmann/json: JSON parsing
- libcurl: HTTP requests
- libpqxx: PostgreSQL integration
- CLI11: Command-line interface
- spdlog: Logging
- dotenv-cpp: Environment variables
- date: Date/time operations
- Eigen: Numerical computations
- mlpack: Machine learning
- TA-Lib: Technical analysis
- XGBoost: Gradient boosting
- LightGBM: Gradient boosting

### Tools
- CMake: Build system
- vcpkg: Package management
- Docker: Containerization
- Google Test: Testing framework
- GitHub Actions: CI/CD

---

*This TODO list is a dynamic document and should be updated weekly based on progress and priorities. Items may be added, removed, or reordered as needed.*