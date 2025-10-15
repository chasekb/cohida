# Coinbase Historical Data Retrieval Project - TODO

## Project Overview
Python application to retrieve historical cryptocurrency data from Coinbase using coinbase-advanced-py library, with testing and deployment via podman-compose.

## Current Status
All major development phases completed. Project is functional with identified issues requiring resolution.

## Active Issues
- [ ] **CRITICAL**: Fix retrieve-all functionality for 1m granularity
  - Error: "Start date must be before end date" in _find_earliest_available_data
  - API Error: "number of candles requested should be less than 350"
  - Root cause: Chunk size calculation issue for small granularities (1-minute)

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