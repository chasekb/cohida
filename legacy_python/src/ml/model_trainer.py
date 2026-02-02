"""
Model training orchestration for ML pipeline.

Handles XGBoost/LightGBM training, hyperparameter tuning,
cross-validation, and training session tracking.
"""

from __future__ import annotations

from dataclasses import dataclass, asdict
from typing import Optional, Dict, Any, List, Callable
from datetime import datetime
import pandas as pd
import numpy as np
import xgboost as xgb
import lightgbm as lgb
from sklearn.model_selection import GridSearchCV, RandomizedSearchCV
import structlog

from .preprocessor import Preprocessor, PreprocessorConfig
from .utils.data_splitter import DataSplitter
from .model_registry import ModelRegistry, ModelMetadata

logger = structlog.get_logger(__name__)


@dataclass
class TrainingConfig:
    """Configuration for model training."""
    
    # Model type: 'xgboost' or 'lightgbm'
    model_type: str = 'xgboost'
    
    # Task type: 'classification' or 'regression'
    task_type: str = 'classification'
    
    # Target column name
    target_col: str = 'target'
    
    # Feature columns (None = all except target and excludes)
    feature_cols: Optional[List[str]] = None
    
    # Columns to exclude from features
    exclude_cols: List[str] = None
    
    # Hyperparameter tuning: 'grid', 'random', or None
    tuning_method: Optional[str] = None
    
    # Cross-validation folds
    cv_folds: int = 5
    
    # Model hyperparameters (default if no tuning)
    hyperparameters: Dict[str, Any] = None
    
    # Hyperparameter grid for tuning
    param_grid: Optional[Dict[str, List[Any]]] = None
    
    # Number of iterations for random search
    n_iter_random: int = 20
    
    # Early stopping rounds
    early_stopping_rounds: int = 50
    
    # Evaluation metric
    eval_metric: str = 'logloss'
    
    # Verbose training
    verbose: bool = False
    
    def __post_init__(self):
        if self.exclude_cols is None:
            self.exclude_cols = ['timestamp', 'symbol']
        
        if self.hyperparameters is None:
            self.hyperparameters = self._get_default_hyperparameters()
    
    def _get_default_hyperparameters(self) -> Dict[str, Any]:
        """Get default hyperparameters for model type."""
        if self.model_type == 'xgboost':
            if self.task_type == 'classification':
                return {
                    'objective': 'binary:logistic',
                    'max_depth': 6,
                    'learning_rate': 0.1,
                    'n_estimators': 100,
                    'subsample': 0.8,
                    'colsample_bytree': 0.8,
                    'random_state': 42,
                }
            else:  # regression
                return {
                    'objective': 'reg:squarederror',
                    'max_depth': 6,
                    'learning_rate': 0.1,
                    'n_estimators': 100,
                    'subsample': 0.8,
                    'colsample_bytree': 0.8,
                    'random_state': 42,
                }
        elif self.model_type == 'lightgbm':
            if self.task_type == 'classification':
                return {
                    'objective': 'binary',
                    'max_depth': 6,
                    'learning_rate': 0.1,
                    'n_estimators': 100,
                    'subsample': 0.8,
                    'colsample_bytree': 0.8,
                    'random_state': 42,
                }
            else:  # regression
                return {
                    'objective': 'regression',
                    'max_depth': 6,
                    'learning_rate': 0.1,
                    'n_estimators': 100,
                    'subsample': 0.8,
                    'colsample_bytree': 0.8,
                    'random_state': 42,
                }
        else:
            raise ValueError(f"Unknown model type: {self.model_type}")


@dataclass
class TrainingResult:
    """Result of model training."""
    
    model: Any
    preprocessor: Preprocessor
    metrics: Dict[str, float]
    feature_names: List[str]
    feature_importance: Dict[str, float]
    training_time: float
    best_params: Optional[Dict[str, Any]] = None
    cv_scores: Optional[List[float]] = None


class ModelTrainer:
    """
    Orchestrates ML model training with XGBoost/LightGBM.
    
    Handles:
    - Model training with configurable hyperparameters
    - Hyperparameter tuning (Grid/Random search)
    - Time-series cross-validation
    - Feature importance analysis
    - Training session tracking
    """
    
    def __init__(
        self,
        config: Optional[TrainingConfig] = None,
        preprocessor_config: Optional[PreprocessorConfig] = None,
    ):
        """
        Initialize model trainer.
        
        Args:
            config: Training configuration
            preprocessor_config: Preprocessing configuration
        """
        self.config = config or TrainingConfig()
        self.preprocessor_config = preprocessor_config or PreprocessorConfig()
        self.data_splitter = DataSplitter()
    
    def train(
        self,
        df: pd.DataFrame,
        symbol: str,
        granularity: str,
        registry: Optional[ModelRegistry] = None,
        model_version: Optional[str] = None,
        progress_callback: Optional[Callable[[str], None]] = None,
    ) -> TrainingResult:
        """
        Train model on provided dataframe.
        
        Args:
            df: Training dataframe with features and target
            symbol: Trading symbol
            granularity: Data granularity
            registry: Model registry for saving trained model
            model_version: Version string for model (auto-generated if None)
            progress_callback: Optional callback for progress updates
        
        Returns:
            TrainingResult with trained model and metrics
        """
        start_time = datetime.utcnow()
        
        # Progress update
        if progress_callback:
            progress_callback("Preparing data...")
        
        # Prepare features and target
        X, y, feature_names = self._prepare_features_target(df)
        
        logger.info(
            "Training data prepared",
            n_samples=len(X),
            n_features=len(feature_names),
            target_col=self.config.target_col,
        )
        
        # Split data
        if progress_callback:
            progress_callback("Splitting data into train/val/test...")
        
        split = self.data_splitter.split(X)
        X_train, X_val, X_test = split.train, split.val, split.test
        y_train = y.loc[X_train.index]
        y_val = y.loc[X_val.index]
        y_test = y.loc[X_test.index]
        
        logger.info("Data split completed", split_summary=split.summary())
        
        # Preprocess features
        if progress_callback:
            progress_callback("Preprocessing features...")
        
        preprocessor = Preprocessor(self.preprocessor_config)
        X_train_scaled = preprocessor.fit_transform(X_train)
        X_val_scaled = preprocessor.transform(X_val)
        X_test_scaled = preprocessor.transform(X_test)
        
        # Train model
        if progress_callback:
            progress_callback("Training model...")
        
        if self.config.tuning_method:
            model, best_params, cv_scores = self._train_with_tuning(
                X_train_scaled, y_train, X_val_scaled, y_val, progress_callback
            )
        else:
            model = self._train_model(X_train_scaled, y_train, X_val_scaled, y_val)
            best_params = None
            cv_scores = None
        
        # Evaluate model
        if progress_callback:
            progress_callback("Evaluating model...")
        
        metrics = self._evaluate_model(model, X_test_scaled, y_test)
        
        # Get feature importance
        feature_importance = self._get_feature_importance(model, feature_names)
        
        # Calculate training time
        end_time = datetime.utcnow()
        training_time = (end_time - start_time).total_seconds()
        
        logger.info(
            "Model training completed",
            training_time=f"{training_time:.2f}s",
            metrics=metrics,
        )
        
        # Save model to registry if provided
        if registry:
            if progress_callback:
                progress_callback("Saving model to registry...")
            
            self._save_to_registry(
                model=model,
                preprocessor=preprocessor,
                registry=registry,
                symbol=symbol,
                granularity=granularity,
                feature_names=feature_names,
                metrics=metrics,
                best_params=best_params or self.config.hyperparameters,
                version=model_version,
            )
        
        return TrainingResult(
            model=model,
            preprocessor=preprocessor,
            metrics=metrics,
            feature_names=feature_names,
            feature_importance=feature_importance,
            training_time=training_time,
            best_params=best_params,
            cv_scores=cv_scores,
        )
    
    def _prepare_features_target(
        self, df: pd.DataFrame
    ) -> tuple[pd.DataFrame, pd.Series, List[str]]:
        """Prepare features and target from dataframe."""
        if self.config.target_col not in df.columns:
            raise ValueError(f"Target column '{self.config.target_col}' not found")
        
        # Get target
        y = df[self.config.target_col].copy()
        
        # Determine feature columns
        if self.config.feature_cols:
            feature_names = self.config.feature_cols
        else:
            # Use all columns except target and excludes
            exclude = set(self.config.exclude_cols) | {self.config.target_col}
            feature_names = [col for col in df.columns if col not in exclude]
        
        # Get features
        X = df[feature_names].copy()
        
        return X, y, feature_names
    
    def _train_model(
        self,
        X_train: pd.DataFrame,
        y_train: pd.Series,
        X_val: pd.DataFrame,
        y_val: pd.Series,
    ) -> Any:
        """Train model with fixed hyperparameters."""
        if self.config.model_type == 'xgboost':
            model = xgb.XGBClassifier(**self.config.hyperparameters) \
                if self.config.task_type == 'classification' \
                else xgb.XGBRegressor(**self.config.hyperparameters)
            
            model.fit(
                X_train, y_train,
                eval_set=[(X_val, y_val)],
                early_stopping_rounds=self.config.early_stopping_rounds,
                verbose=self.config.verbose,
            )
        
        elif self.config.model_type == 'lightgbm':
            model = lgb.LGBMClassifier(**self.config.hyperparameters) \
                if self.config.task_type == 'classification' \
                else lgb.LGBMRegressor(**self.config.hyperparameters)
            
            model.fit(
                X_train, y_train,
                eval_set=[(X_val, y_val)],
                callbacks=[lgb.early_stopping(self.config.early_stopping_rounds)],
            )
        
        else:
            raise ValueError(f"Unknown model type: {self.config.model_type}")
        
        return model
    
    def _train_with_tuning(
        self,
        X_train: pd.DataFrame,
        y_train: pd.Series,
        X_val: pd.DataFrame,
        y_val: pd.Series,
        progress_callback: Optional[Callable[[str], None]] = None,
    ) -> tuple[Any, Dict[str, Any], List[float]]:
        """Train model with hyperparameter tuning."""
        if progress_callback:
            progress_callback(f"Running {self.config.tuning_method} search...")
        
        # Create base model
        if self.config.model_type == 'xgboost':
            base_model = xgb.XGBClassifier(random_state=42) \
                if self.config.task_type == 'classification' \
                else xgb.XGBRegressor(random_state=42)
        elif self.config.model_type == 'lightgbm':
            base_model = lgb.LGBMClassifier(random_state=42) \
                if self.config.task_type == 'classification' \
                else lgb.LGBMRegressor(random_state=42)
        else:
            raise ValueError(f"Unknown model type: {self.config.model_type}")
        
        # Get parameter grid
        param_grid = self.config.param_grid or self._get_default_param_grid()
        
        # Run hyperparameter search
        if self.config.tuning_method == 'grid':
            search = GridSearchCV(
                base_model,
                param_grid,
                cv=self.config.cv_folds,
                scoring='accuracy' if self.config.task_type == 'classification' else 'neg_mean_squared_error',
                verbose=1 if self.config.verbose else 0,
                n_jobs=-1,
            )
        elif self.config.tuning_method == 'random':
            search = RandomizedSearchCV(
                base_model,
                param_grid,
                n_iter=self.config.n_iter_random,
                cv=self.config.cv_folds,
                scoring='accuracy' if self.config.task_type == 'classification' else 'neg_mean_squared_error',
                verbose=1 if self.config.verbose else 0,
                n_jobs=-1,
                random_state=42,
            )
        else:
            raise ValueError(f"Unknown tuning method: {self.config.tuning_method}")
        
        # Fit search
        search.fit(X_train, y_train)
        
        # Get best model and parameters
        best_model = search.best_estimator_
        best_params = search.best_params_
        cv_scores = search.cv_results_['mean_test_score'].tolist()
        
        logger.info(
            "Hyperparameter tuning completed",
            best_params=best_params,
            best_score=search.best_score_,
        )
        
        return best_model, best_params, cv_scores
    
    def _evaluate_model(
        self, model: Any, X_test: pd.DataFrame, y_test: pd.Series
    ) -> Dict[str, float]:
        """Evaluate model on test set."""
        from sklearn.metrics import (
            accuracy_score, precision_score, recall_score, f1_score, roc_auc_score,
            mean_squared_error, mean_absolute_error, r2_score
        )
        
        y_pred = model.predict(X_test)
        
        metrics = {}
        
        if self.config.task_type == 'classification':
            metrics['accuracy'] = accuracy_score(y_test, y_pred)
            metrics['precision'] = precision_score(y_test, y_pred, average='binary', zero_division=0)
            metrics['recall'] = recall_score(y_test, y_pred, average='binary', zero_division=0)
            metrics['f1_score'] = f1_score(y_test, y_pred, average='binary', zero_division=0)
            
            # ROC AUC if probabilities available
            if hasattr(model, 'predict_proba'):
                y_proba = model.predict_proba(X_test)[:, 1]
                metrics['roc_auc'] = roc_auc_score(y_test, y_proba)
        
        else:  # regression
            metrics['mse'] = mean_squared_error(y_test, y_pred)
            metrics['rmse'] = np.sqrt(metrics['mse'])
            metrics['mae'] = mean_absolute_error(y_test, y_pred)
            metrics['r2_score'] = r2_score(y_test, y_pred)
            
            # MAPE (avoid division by zero)
            mask = y_test != 0
            if mask.sum() > 0:
                metrics['mape'] = np.mean(np.abs((y_test[mask] - y_pred[mask]) / y_test[mask])) * 100
        
        return metrics
    
    def _get_feature_importance(
        self, model: Any, feature_names: List[str]
    ) -> Dict[str, float]:
        """Extract feature importance from model."""
        try:
            if hasattr(model, 'feature_importances_'):
                importances = model.feature_importances_
                return dict(zip(feature_names, importances.tolist()))
        except Exception as e:
            logger.warning(f"Could not extract feature importance: {e}")
        
        return {}
    
    def _get_default_param_grid(self) -> Dict[str, List[Any]]:
        """Get default parameter grid for hyperparameter tuning."""
        if self.config.model_type == 'xgboost':
            return {
                'max_depth': [3, 6, 9],
                'learning_rate': [0.01, 0.1, 0.3],
                'n_estimators': [50, 100, 200],
                'subsample': [0.7, 0.8, 0.9],
                'colsample_bytree': [0.7, 0.8, 0.9],
            }
        elif self.config.model_type == 'lightgbm':
            return {
                'max_depth': [3, 6, 9],
                'learning_rate': [0.01, 0.1, 0.3],
                'n_estimators': [50, 100, 200],
                'subsample': [0.7, 0.8, 0.9],
                'colsample_bytree': [0.7, 0.8, 0.9],
            }
        else:
            return {}
    
    def _save_to_registry(
        self,
        model: Any,
        preprocessor: Preprocessor,
        registry: ModelRegistry,
        symbol: str,
        granularity: str,
        feature_names: List[str],
        metrics: Dict[str, float],
        best_params: Dict[str, Any],
        version: Optional[str] = None,
    ) -> str:
        """Save trained model to registry."""
        # Generate model ID
        model_id = registry.generate_model_id(
            model_type=self.config.model_type,
            symbol=symbol,
            version=version,
        )
        
        # Create metadata
        metadata = ModelMetadata(
            model_id=model_id,
            model_type=self.config.model_type,
            version=version or 'auto',
            symbol=symbol,
            granularity=granularity,
            created_at=datetime.utcnow().isoformat(),
            hyperparameters=best_params,
            metrics=metrics,
            feature_names=feature_names,
            preprocessor_config=preprocessor.get_config(),
        )
        
        # Save to registry
        registry.save_model(model, metadata, preprocessor)
        
        logger.info("Model saved to registry", model_id=model_id)
        
        return model_id



