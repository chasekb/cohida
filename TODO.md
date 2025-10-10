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

## Project Status: ✅ CORE COMPLETED - ENHANCING WITH INTRADAY ANALYSIS
All core development tasks and advanced features have been successfully completed. The project now includes comprehensive intraday return probability analysis capabilities.

### Phase 8: Intraday Return Probability Analysis
- [ ] **Intraday Analysis Data Models**
  - [ ] Create `IntradayReturnData` model for storing calculated returns
  - [ ] Create `TimePeriodAnalysis` model for different intraday periods
  - [ ] Create `ProbabilityMetrics` model for positive/negative return probabilities
  - [ ] Create `IntradayPattern` model for pattern recognition results

- [ ] **Return Calculation Engine**
  - [ ] Implement `ReturnCalculator` class for various return calculations
  - [ ] Add support for different return types (simple, log, percentage)
  - [ ] Implement rolling return calculations for different time windows
  - [ ] Add volatility calculations (standard deviation, variance)

- [ ] **Time Period Analyzer**
  - [ ] Create `IntradayTimePeriodAnalyzer` for different market hours
  - [ ] Implement analysis for pre-market (4:00-9:30 ET)
  - [ ] Implement analysis for regular market (9:30-16:00 ET)
  - [ ] Implement analysis for after-hours (16:00-20:00 ET)
  - [ ] Add support for custom time period definitions
  - [ ] Implement weekend/holiday analysis patterns

- [ ] **Probability Analysis Engine**
  - [ ] Create `ProbabilityAnalyzer` class for return probability calculations
  - [ ] Implement positive return probability calculations by time period
  - [ ] Implement negative return probability calculations by time period
  - [ ] Add confidence interval calculations
  - [ ] Implement statistical significance testing
  - [ ] Add trend analysis (increasing/decreasing probabilities over time)

- [ ] **Pattern Recognition System**
  - [ ] Create `PatternRecognizer` for identifying intraday patterns
  - [ ] Implement gap analysis (opening vs previous close)
  - [ ] Add momentum pattern detection
  - [ ] Implement reversal pattern identification
  - [ ] Add volume correlation analysis

- [ ] **Statistical Analysis Tools**
  - [ ] Create `StatisticalAnalyzer` for advanced statistical metrics
  - [ ] Implement correlation analysis between time periods
  - [ ] Add regression analysis for trend prediction
  - [ ] Implement Monte Carlo simulation for probability modeling
  - [ ] Add backtesting framework for strategy validation

- [ ] **Data Aggregation & Reporting**
  - [ ] Create `IntradayReportGenerator` for comprehensive reports
  - [ ] Implement daily probability summaries
  - [ ] Add weekly/monthly trend analysis
  - [ ] Create comparative analysis across different symbols
  - [ ] Implement risk-adjusted return metrics

- [ ] **Visualization & Export**
  - [ ] Create `IntradayVisualizer` for chart generation
  - [ ] Implement probability heat maps by time period
  - [ ] Add return distribution histograms
  - [ ] Create trend line charts for probability changes
  - [ ] Implement interactive dashboards

- [ ] **Database Schema Extensions**
  - [ ] Create `intraday_returns` table for calculated return data
  - [ ] Create `time_period_analysis` table for period-specific metrics
  - [ ] Create `probability_metrics` table for probability calculations
  - [ ] Add indexes for efficient time-based queries
  - [ ] Implement data partitioning by time periods

- [ ] **CLI Enhancements**
  - [ ] Add `--analyze-intraday` flag to CLI
  - [ ] Implement `--time-period` parameter for specific periods
  - [ ] Add `--probability-threshold` for filtering results
  - [ ] Create `--export-analysis` for CSV output of analysis results
  - [ ] Add `--visualize` flag for generating charts

- [ ] **API Extensions**
  - [ ] Extend `HistoricalDataRetriever` with analysis capabilities
  - [ ] Add `analyze_intraday_returns()` method
  - [ ] Implement `get_probability_metrics()` method
  - [ ] Add `generate_intraday_report()` method
  - [ ] Create `export_analysis_data()` method

- [ ] **Testing & Validation**
  - [ ] Create unit tests for return calculation engine
  - [ ] Add integration tests for probability analysis
  - [ ] Implement end-to-end tests for full analysis workflow
  - [ ] Create performance tests for large dataset analysis
  - [ ] Add validation tests for statistical accuracy

- [ ] **Documentation & Examples**
  - [ ] Update README with intraday analysis features
  - [ ] Create analysis methodology documentation
  - [ ] Add example analysis scripts
  - [ ] Document probability calculation formulas
  - [ ] Create user guide for interpretation of results

## Analysis Methodology

### Return Calculations
- **Simple Returns**: (Close - Open) / Open
- **Log Returns**: ln(Close / Open)
- **Percentage Returns**: ((Close - Open) / Open) * 100
- **Rolling Returns**: Returns over configurable time windows

### Time Periods Analyzed
- **Pre-Market**: 4:00 AM - 9:30 AM ET
- **Regular Market**: 9:30 AM - 4:00 PM ET
- **After-Hours**: 4:00 PM - 8:00 PM ET
- **Custom Periods**: User-defined time ranges
- **Intraday Intervals**: 15min, 30min, 1hr, 2hr, 4hr periods

### Probability Metrics
- **Positive Return Probability**: P(Return > 0) for each time period
- **Negative Return Probability**: P(Return < 0) for each time period
- **Confidence Intervals**: 95% and 99% confidence levels
- **Statistical Significance**: p-values for probability differences
- **Trend Analysis**: Changes in probabilities over time

### Output Formats
- **CSV Files**: Detailed analysis results in `outputs/` folder
- **JSON Reports**: Structured analysis data
- **Visual Charts**: Probability heat maps and trend charts
- **Summary Tables**: Key metrics and insights

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
- All analysis outputs should be written to `outputs/` folder as CSV files
- Implement write-as-read capability for analysis results
