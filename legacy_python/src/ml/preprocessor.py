"""
Data preprocessing utilities for ML pipeline.

Handles feature scaling, normalization, missing data imputation,
and outlier detection/treatment for time-series financial data.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, List, Dict, Any
import pandas as pd
import numpy as np
from sklearn.preprocessing import StandardScaler, RobustScaler, MinMaxScaler
import structlog

logger = structlog.get_logger(__name__)


@dataclass
class PreprocessorConfig:
    """Configuration for data preprocessing."""
    
    # Scaling strategy: 'standard', 'robust', 'minmax', or None
    scaler_type: str = 'robust'
    
    # Missing data handling: 'drop', 'ffill', 'bfill', 'interpolate', 'mean'
    missing_strategy: str = 'ffill'
    
    # Outlier detection: 'iqr', 'zscore', or None
    outlier_method: Optional[str] = 'iqr'
    
    # Outlier treatment: 'clip', 'remove', 'winsorize'
    outlier_treatment: str = 'clip'
    
    # Outlier parameters
    iqr_multiplier: float = 3.0  # For IQR method
    zscore_threshold: float = 3.0  # For Z-score method
    
    # Feature selection (columns to exclude from scaling)
    exclude_from_scaling: List[str] = None
    
    def __post_init__(self):
        if self.exclude_from_scaling is None:
            self.exclude_from_scaling = ['timestamp', 'symbol', 'hour', 'dayofweek', 'month']


class Preprocessor:
    """
    Preprocesses features for ML training and inference.
    
    Handles:
    - Feature scaling/normalization
    - Missing data imputation
    - Outlier detection and treatment
    - Ensures no lookahead bias in time-series preprocessing
    """
    
    def __init__(self, config: Optional[PreprocessorConfig] = None):
        """
        Initialize preprocessor with configuration.
        
        Args:
            config: Preprocessing configuration (uses defaults if None)
        """
        self.config = config or PreprocessorConfig()
        self.scaler = None
        self.fitted = False
        self.feature_names: Optional[List[str]] = None
        self.outlier_bounds: Dict[str, tuple] = {}
    
    def fit(self, df: pd.DataFrame) -> Preprocessor:
        """
        Fit preprocessor on training data.
        
        Args:
            df: Training dataframe with features
        
        Returns:
            Self for method chaining
        """
        if df.empty:
            raise ValueError("Cannot fit on empty dataframe")
        
        # Store feature names
        self.feature_names = [
            col for col in df.columns
            if col not in self.config.exclude_from_scaling
        ]
        
        if not self.feature_names:
            logger.warning("No features to scale after exclusions")
            self.fitted = True
            return self
        
        # Select features for scaling
        X = df[self.feature_names].copy()
        
        # Handle missing values
        X = self._handle_missing_data(X, fit=True)
        
        # Fit outlier bounds
        if self.config.outlier_method:
            self._fit_outlier_bounds(X)
        
        # Fit scaler
        if self.config.scaler_type:
            self.scaler = self._create_scaler()
            self.scaler.fit(X)
            logger.info(
                "Preprocessor fitted",
                scaler=self.config.scaler_type,
                n_features=len(self.feature_names),
            )
        
        self.fitted = True
        return self
    
    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Transform features using fitted preprocessor.
        
        Args:
            df: Dataframe with features to transform
        
        Returns:
            Transformed dataframe
        
        Raises:
            RuntimeError: If preprocessor hasn't been fitted
        """
        if not self.fitted:
            raise RuntimeError("Preprocessor must be fitted before transform")
        
        if df.empty:
            return df.copy()
        
        # Work with a copy
        result = df.copy()
        
        # Get features to transform
        features_to_scale = [
            col for col in self.feature_names
            if col in result.columns
        ]
        
        if not features_to_scale:
            logger.warning("No features to transform")
            return result
        
        # Extract features
        X = result[features_to_scale].copy()
        
        # Handle missing values
        X = self._handle_missing_data(X, fit=False)
        
        # Handle outliers
        if self.config.outlier_method and self.outlier_bounds:
            X = self._treat_outliers(X)
        
        # Scale features
        if self.scaler:
            X_scaled = self.scaler.transform(X)
            X = pd.DataFrame(X_scaled, columns=features_to_scale, index=X.index)
        
        # Update result with transformed features
        result[features_to_scale] = X
        
        return result
    
    def fit_transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Fit preprocessor and transform data in one step.
        
        Args:
            df: Training dataframe
        
        Returns:
            Transformed dataframe
        """
        return self.fit(df).transform(df)
    
    def inverse_transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Inverse transform scaled features back to original scale.
        
        Args:
            df: Dataframe with scaled features
        
        Returns:
            Dataframe with features in original scale
        """
        if not self.fitted or not self.scaler:
            return df.copy()
        
        result = df.copy()
        
        features_to_scale = [
            col for col in self.feature_names
            if col in result.columns
        ]
        
        if features_to_scale:
            X = result[features_to_scale].values
            X_inv = self.scaler.inverse_transform(X)
            result[features_to_scale] = X_inv
        
        return result
    
    def _create_scaler(self):
        """Create scaler based on configuration."""
        if self.config.scaler_type == 'standard':
            return StandardScaler()
        elif self.config.scaler_type == 'robust':
            return RobustScaler()
        elif self.config.scaler_type == 'minmax':
            return MinMaxScaler()
        else:
            raise ValueError(f"Unknown scaler type: {self.config.scaler_type}")
    
    def _handle_missing_data(self, df: pd.DataFrame, fit: bool = False) -> pd.DataFrame:
        """
        Handle missing data according to configuration.
        
        Args:
            df: Dataframe with potential missing values
            fit: If True, log statistics about missing data
        
        Returns:
            Dataframe with missing values handled
        """
        result = df.copy()
        
        # Check for missing data
        missing_count = result.isnull().sum().sum()
        if missing_count == 0:
            return result
        
        if fit:
            missing_per_col = result.isnull().sum()
            logger.info(
                "Missing data detected",
                total_missing=missing_count,
                columns_with_missing=missing_per_col[missing_per_col > 0].to_dict(),
            )
        
        # Apply missing data strategy
        if self.config.missing_strategy == 'drop':
            result = result.dropna()
        elif self.config.missing_strategy == 'ffill':
            result = result.fillna(method='ffill')
        elif self.config.missing_strategy == 'bfill':
            result = result.fillna(method='bfill')
        elif self.config.missing_strategy == 'interpolate':
            result = result.interpolate(method='linear', limit_direction='both')
        elif self.config.missing_strategy == 'mean':
            result = result.fillna(result.mean())
        
        # Final check - if any NaN remain, drop them
        remaining_missing = result.isnull().sum().sum()
        if remaining_missing > 0:
            logger.warning(
                f"Dropping {remaining_missing} rows with remaining missing values"
            )
            result = result.dropna()
        
        return result
    
    def _fit_outlier_bounds(self, df: pd.DataFrame) -> None:
        """
        Fit outlier detection bounds on training data.
        
        Args:
            df: Training dataframe
        """
        for col in df.columns:
            if df[col].dtype in [np.float64, np.float32, np.int64, np.int32]:
                if self.config.outlier_method == 'iqr':
                    Q1 = df[col].quantile(0.25)
                    Q3 = df[col].quantile(0.75)
                    IQR = Q3 - Q1
                    lower = Q1 - self.config.iqr_multiplier * IQR
                    upper = Q3 + self.config.iqr_multiplier * IQR
                    self.outlier_bounds[col] = (lower, upper)
                
                elif self.config.outlier_method == 'zscore':
                    mean = df[col].mean()
                    std = df[col].std()
                    lower = mean - self.config.zscore_threshold * std
                    upper = mean + self.config.zscore_threshold * std
                    self.outlier_bounds[col] = (lower, upper)
        
        logger.info(
            "Outlier bounds fitted",
            method=self.config.outlier_method,
            n_features=len(self.outlier_bounds),
        )
    
    def _treat_outliers(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Treat outliers according to configuration.
        
        Args:
            df: Dataframe with potential outliers
        
        Returns:
            Dataframe with outliers treated
        """
        result = df.copy()
        
        for col, (lower, upper) in self.outlier_bounds.items():
            if col not in result.columns:
                continue
            
            if self.config.outlier_treatment == 'clip':
                result[col] = result[col].clip(lower=lower, upper=upper)
            
            elif self.config.outlier_treatment == 'remove':
                mask = (result[col] >= lower) & (result[col] <= upper)
                result = result[mask]
            
            elif self.config.outlier_treatment == 'winsorize':
                # Winsorization: replace outliers with boundary values
                result.loc[result[col] < lower, col] = lower
                result.loc[result[col] > upper, col] = upper
        
        return result
    
    def get_config(self) -> Dict[str, Any]:
        """Return preprocessing configuration as dict."""
        return {
            'scaler_type': self.config.scaler_type,
            'missing_strategy': self.config.missing_strategy,
            'outlier_method': self.config.outlier_method,
            'outlier_treatment': self.config.outlier_treatment,
            'iqr_multiplier': self.config.iqr_multiplier,
            'zscore_threshold': self.config.zscore_threshold,
            'feature_names': self.feature_names,
        }



