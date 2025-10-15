# Coinbase Historical Data Retrieval Project - TODO

## Project Overview
Python application to retrieve historical cryptocurrency data from Coinbase using coinbase-advanced-py library, with testing and deployment via podman-compose. Now expanding to include machine learning trend analysis capabilities.

## Current Status
All major development phases completed. Project is functional with identified issues requiring resolution. New ML functionality planned.

## Active Issues
- [x] **CRITICAL**: Fix retrieve-all functionality for 1m granularity
  - Fixed: adjusted per-request limit to 299 candles, added robust chunk error handling (including StopIteration), and enforced safe chunk durations to avoid boundary errors.
  - Verified: unit tests `tests/unit/test_retrieve_all.py` all pass inside podman container.

---

## Machine Learning Trend Analysis - Development Plan

### Objectives
Build ML capabilities to analyze and predict cryptocurrency price trends using historical OHLCV data:
- Trend Classification (uptrend/downtrend/sideways)
- Price Prediction (short-term forecasting)
- Volatility Analysis
- Pattern Recognition
- Risk Scoring

### Architecture

**New Module Structure:**
```
src/ml/
├── feature_engineer.py          # Feature extraction from OHLCV data
├── preprocessor.py              # Data preprocessing & normalization
├── model_trainer.py             # Model training orchestration
├── trend_predictor.py           # Inference & prediction
├── model_registry.py            # Model versioning & storage
├── evaluation_metrics.py        # Performance evaluation
└── technical_indicators.py      # Technical analysis calculations

src/ml/models/
├── trend_classifier.py          # Classification models
├── price_forecaster.py          # Regression models
├── volatility_analyzer.py       # Volatility prediction
└── ensemble_model.py            # Ensemble strategies

src/ml/utils/
├── data_splitter.py            # Train/test/validation splits
├── window_generator.py         # Time-series window generation
└── model_persistence.py        # Save/load model artifacts
```

**Database Extensions:**
- `mse.ml_features`: Computed features cache
- `mse.ml_predictions`: Model predictions with timestamps
- `mse.ml_models`: Model metadata & versioning
- `mse.ml_training_runs`: Training session tracking

### Phase 1: Foundation (Core Infrastructure)
- [ ] Create src/ml/ module structure with __init__.py
- [ ] Implement TechnicalIndicators class
  - SMA, EMA, RSI, MACD, Bollinger Bands
  - Volume indicators (OBV, VWAP)
  - Volatility indicators (ATR, True Range)
- [ ] Implement FeatureEngineer class
  - Technical indicator computation
  - Lag features, rolling statistics
  - Temporal features (day, hour, month)
- [ ] Extend DatabaseSchema for ML tables
  - ml_features table schema
  - ml_predictions table schema
  - ml_models table schema
  - ml_training_runs table schema
- [ ] Create database migrations for new tables
- [ ] Update requirements.txt with ML dependencies
  - scikit-learn, xgboost, lightgbm
  - ta (technical analysis library)
  - pandas, numpy upgrades

### Phase 2: Training Pipeline
- [ ] Implement DataSplitter class
  - Time-based train/val/test splits (70/15/15)
  - Walk-forward validation
  - No lookahead bias
- [ ] Implement Preprocessor class
  - Feature scaling/normalization
  - Missing data handling
  - Outlier detection
- [ ] Implement ModelTrainer class
  - XGBoost integration
  - Hyperparameter tuning (GridSearch)
  - Cross-validation (time-series CV)
  - Training session tracking
- [ ] Implement ModelRegistry class
  - Model versioning (semantic versioning)
  - Metadata storage (hyperparameters, metrics)
  - Save/load model artifacts
  - Model comparison utilities
- [ ] Create training CLI commands
  - `ml-train` command with symbol/granularity/model options
  - Progress tracking and logging
  - Training report generation

### Phase 3: Prediction Pipeline
- [ ] Implement TrendPredictor class
  - Load trained models
  - Real-time feature computation
  - Trend classification (UP/DOWN/SIDEWAYS)
  - Confidence scores
- [ ] Implement PriceForecaster class
  - Price prediction with horizons (1, 6, 24 hours)
  - Uncertainty quantification
  - Multi-step ahead forecasting
- [ ] Create prediction CLI commands
  - `ml-predict` for single predictions
  - `ml-predict-batch` for multiple symbols
  - Output to console, CSV, JSON
- [ ] Implement prediction storage
  - Write predictions to ml_predictions table
  - Write-as-predict capability
  - Historical prediction tracking

### Phase 4: Evaluation & Monitoring
- [ ] Implement EvaluationMetrics class
  - Classification metrics (accuracy, precision, recall, F1)
  - Regression metrics (MAE, RMSE, MAPE, R²)
  - Trading metrics (Sharpe ratio, max drawdown)
  - Directional accuracy
- [ ] Create backtesting framework
  - Walk-forward validation
  - Out-of-sample testing
  - Baseline comparison (buy-and-hold, naive)
- [ ] Generate evaluation reports
  - Confusion matrices
  - Feature importance plots
  - Prediction vs actual charts
  - Performance summary tables
- [ ] Add model performance CLI commands
  - `ml-evaluate` for model assessment
  - `ml-models` to list available models
  - `ml-features` for feature importance

### Phase 5: Advanced Models (Optional)
- [ ] Implement LSTM models for trend prediction
  - Sequential model architecture
  - Training pipeline integration
  - Multi-step forecasting
- [ ] Implement ensemble strategies
  - Model stacking
  - Voting classifiers
  - Weighted averaging
- [ ] Add VolatilityAnalyzer class
  - ATR prediction
  - Risk level classification
  - Volatility forecasting
- [ ] Implement trading signal generation
  - BUY/SELL/HOLD recommendations
  - Risk-adjusted signals
  - Signal confidence scores

### Phase 6: Production Features (Optional)
- [ ] Model retraining automation
  - Scheduled retraining jobs
  - Performance degradation detection
  - Automatic model updates
- [ ] Feature caching optimization
  - Incremental feature computation
  - Redis/in-memory caching
  - Cache invalidation strategy
- [ ] API endpoint for predictions
  - REST API with FastAPI
  - Real-time prediction serving
  - Rate limiting and authentication
- [ ] Real-time streaming predictions
  - WebSocket support
  - Live market data integration
  - Streaming feature computation

### MVP Scope (Recommended Starting Point)
**Focus:** Trend classification using XGBoost on BTC-USD with 1h granularity

**Minimum Implementation:**
- Phases 1-3 only
- Single model (XGBoost trend classifier)
- Basic technical indicators (SMA, EMA, RSI, MACD)
- CLI commands for train/predict/evaluate
- Skip deep learning (LSTM) initially

**Success Criteria:**
- Trend classification with >55% directional accuracy
- Price prediction with <5% MAPE on 1-hour horizon
- Training pipeline producing reproducible models
- CLI integration functional

### Feature Engineering Details

**Technical Indicators (Priority 1):**
- SMA: 7, 14, 30, 50, 200-period
- EMA: 12, 26, 50-period
- RSI (14-period)
- MACD (12, 26, 9)
- Bollinger Bands (20-period, 2 std dev)
- ATR (14-period)
- Volume MA (20-period)

**Derived Features (Priority 2):**
- Price returns (1, 6, 24-period)
- Log returns
- Rolling volatility (20-period)
- High-Low range
- Volume rate of change
- Day of week, hour of day

**Advanced Features (Priority 3):**
- OBV (On-Balance Volume)
- VWAP (Volume-Weighted Average Price)
- Candlestick patterns
- Support/resistance levels
- Momentum indicators

### Model Selection

**Primary Model: XGBoost**
- Best performance-to-effort ratio
- Handles non-linear relationships
- Feature importance analysis
- Fast training and inference
- Built-in regularization

**Alternative: LightGBM**
- Faster training on large datasets
- Lower memory usage
- Similar performance to XGBoost

**Deep Learning: LSTM (Phase 5)**
- Captures temporal dependencies
- Better for longer sequences
- Requires more data and tuning
- Higher computational cost

### CLI Commands (Planned)

```bash
# Train models
python src/cli.py ml-train BTC-USD --granularity 1h --model xgboost
python src/cli.py ml-train BTC-USD --granularity 1h --features all

# Predict trend
python src/cli.py ml-predict BTC-USD --granularity 1h --horizon 6
python src/cli.py ml-predict BTC-USD --output-format json

# Batch predictions
python src/cli.py ml-predict-batch BTC-USD ETH-USD --save-csv

# Evaluate model
python src/cli.py ml-evaluate --model-id xgb_trend_v1.0 --test-period 30d

# List available models
python src/cli.py ml-models --symbol BTC-USD

# Feature importance
python src/cli.py ml-features --model-id xgb_trend_v1.0

# Training history
python src/cli.py ml-history --symbol BTC-USD --limit 10
```

### Dependencies to Add

```
# Core ML
scikit-learn>=1.3.0
xgboost>=2.0.0
lightgbm>=4.0.0

# Feature Engineering
ta>=0.11.0
pandas>=2.0.0
numpy>=1.24.0

# Visualization
matplotlib>=3.7.0
seaborn>=0.12.0

# Model Management
joblib>=1.3.0

# Optional (Deep Learning)
tensorflow>=2.14.0
# OR
torch>=2.0.0

# Optional (Advanced)
prophet>=1.1.0
mlflow>=2.8.0
```

### Estimated Effort

**MVP (Phases 1-3):**
- Phase 1: 8-12 hours
- Phase 2: 10-15 hours
- Phase 3: 6-8 hours
- **Total: 24-35 hours**

**Full Implementation (All Phases):**
- Phases 1-6: 48-70 hours

### Risk Considerations

**Data Quality:**
- Handle missing data and gaps in historical data
- Outlier detection and treatment
- Data validation pipelines

**Overfitting:**
- Time-series cross-validation mandatory
- Regularization techniques (L1/L2)
- Out-of-sample testing required

**Lookahead Bias:**
- Strict time-based splits (no shuffling)
- Feature computation with causal constraints
- Walk-forward validation

**Computational:**
- Feature computation can be expensive → implement caching
- Model training time → consider incremental learning
- Real-time prediction latency → pre-compute features

**Market Risk:**
- ML predictions are probabilities, not certainties
- Include confidence intervals in all predictions
- Add risk disclaimers to outputs

### Next Steps

1. Review and approve plan
2. Confirm MVP scope (recommend Phases 1-3)
3. Confirm target symbol (recommend BTC-USD)
4. Confirm granularity (recommend 1h)
5. Begin Phase 1: Foundation implementation

---

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
- **ML Models:** All predictions include confidence scores and uncertainty quantification
- **Feature Engineering:** Cache computed features to avoid redundant calculations
- **Model Versioning:** Use semantic versioning for all trained models