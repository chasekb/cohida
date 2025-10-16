"""
Time-series aware data splitting utilities for ML training.

Implements proper time-based splits to prevent lookahead bias
and supports walk-forward validation strategies.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import List, Tuple, Optional
import pandas as pd
import numpy as np
import structlog

logger = structlog.get_logger(__name__)


@dataclass
class DataSplit:
    """Container for train/val/test data splits."""
    train: pd.DataFrame
    val: pd.DataFrame
    test: pd.DataFrame
    
    @property
    def train_size(self) -> int:
        return len(self.train)
    
    @property
    def val_size(self) -> int:
        return len(self.val)
    
    @property
    def test_size(self) -> int:
        return len(self.test)
    
    def summary(self) -> str:
        """Return summary of split sizes."""
        total = self.train_size + self.val_size + self.test_size
        return (
            f"Split summary:\n"
            f"  Train: {self.train_size} ({100*self.train_size/total:.1f}%)\n"
            f"  Val:   {self.val_size} ({100*self.val_size/total:.1f}%)\n"
            f"  Test:  {self.test_size} ({100*self.test_size/total:.1f}%)\n"
            f"  Total: {total}"
        )


class DataSplitter:
    """
    Time-series aware data splitting for ML training.
    
    Ensures proper temporal ordering to prevent lookahead bias.
    Supports both fixed ratio splits and walk-forward validation.
    """
    
    def __init__(
        self,
        train_ratio: float = 0.7,
        val_ratio: float = 0.15,
        test_ratio: float = 0.15,
    ):
        """
        Initialize data splitter with split ratios.
        
        Args:
            train_ratio: Proportion of data for training (default 0.7)
            val_ratio: Proportion of data for validation (default 0.15)
            test_ratio: Proportion of data for testing (default 0.15)
        
        Raises:
            ValueError: If ratios don't sum to 1.0
        """
        if not abs(train_ratio + val_ratio + test_ratio - 1.0) < 1e-6:
            raise ValueError(
                f"Split ratios must sum to 1.0, got {train_ratio + val_ratio + test_ratio}"
            )
        
        if train_ratio <= 0 or val_ratio <= 0 or test_ratio <= 0:
            raise ValueError("All split ratios must be positive")
        
        self.train_ratio = train_ratio
        self.val_ratio = val_ratio
        self.test_ratio = test_ratio
    
    def split(self, df: pd.DataFrame, ensure_sorted: bool = True) -> DataSplit:
        """
        Split dataframe into train/val/test sets using time-based ordering.
        
        Args:
            df: Input dataframe with datetime index or timestamp column
            ensure_sorted: If True, sort by index/timestamp before splitting
        
        Returns:
            DataSplit containing train, val, and test dataframes
        
        Raises:
            ValueError: If dataframe is empty or too small to split
        """
        if df.empty:
            raise ValueError("Cannot split empty dataframe")
        
        if len(df) < 10:
            raise ValueError(
                f"Dataframe too small to split (size={len(df)}). Need at least 10 rows."
            )
        
        # Work with a copy
        data = df.copy()
        
        # Ensure temporal ordering
        if ensure_sorted:
            if isinstance(data.index, pd.DatetimeIndex):
                data = data.sort_index()
            elif 'timestamp' in data.columns:
                data = data.sort_values('timestamp').reset_index(drop=True)
            else:
                logger.warning(
                    "No datetime index or timestamp column found. "
                    "Assuming data is already in temporal order."
                )
        
        # Calculate split indices
        n = len(data)
        train_end = int(n * self.train_ratio)
        val_end = train_end + int(n * self.val_ratio)
        
        # Perform split (time-ordered, no shuffling)
        train_df = data.iloc[:train_end].copy()
        val_df = data.iloc[train_end:val_end].copy()
        test_df = data.iloc[val_end:].copy()
        
        # Validate splits are non-empty
        if len(train_df) == 0 or len(val_df) == 0 or len(test_df) == 0:
            raise ValueError(
                f"Split resulted in empty partition. "
                f"Sizes: train={len(train_df)}, val={len(val_df)}, test={len(test_df)}"
            )
        
        split = DataSplit(train=train_df, val=val_df, test=test_df)
        logger.info(
            "Data split completed",
            train_size=split.train_size,
            val_size=split.val_size,
            test_size=split.test_size,
        )
        
        return split
    
    def walk_forward_splits(
        self,
        df: pd.DataFrame,
        n_splits: int = 5,
        min_train_size: Optional[int] = None,
    ) -> List[Tuple[pd.DataFrame, pd.DataFrame]]:
        """
        Generate walk-forward validation splits for time-series cross-validation.
        
        Each split uses an expanding training window and a fixed-size test window.
        This prevents lookahead bias while maximizing data utilization.
        
        Args:
            df: Input dataframe with datetime index or timestamp column
            n_splits: Number of CV splits to generate (default 5)
            min_train_size: Minimum training set size (default: 30% of data)
        
        Returns:
            List of (train_df, val_df) tuples for each fold
        
        Raises:
            ValueError: If dataframe is too small for the requested splits
        """
        if df.empty:
            raise ValueError("Cannot split empty dataframe")
        
        if n_splits < 2:
            raise ValueError(f"n_splits must be at least 2, got {n_splits}")
        
        # Work with a copy and ensure temporal ordering
        data = df.copy()
        if isinstance(data.index, pd.DatetimeIndex):
            data = data.sort_index()
        elif 'timestamp' in data.columns:
            data = data.sort_values('timestamp').reset_index(drop=True)
        
        n = len(data)
        
        # Set minimum training size
        if min_train_size is None:
            min_train_size = int(n * 0.3)
        
        if min_train_size >= n:
            raise ValueError(
                f"min_train_size ({min_train_size}) must be less than data size ({n})"
            )
        
        # Calculate test window size for each split
        remaining_data = n - min_train_size
        test_size = remaining_data // n_splits
        
        if test_size < 1:
            raise ValueError(
                f"Not enough data for {n_splits} splits. "
                f"Data size={n}, min_train_size={min_train_size}"
            )
        
        # Generate expanding window splits
        splits = []
        for i in range(n_splits):
            train_end = min_train_size + (i * test_size)
            val_end = train_end + test_size
            
            if val_end > n:
                val_end = n
            
            train_df = data.iloc[:train_end].copy()
            val_df = data.iloc[train_end:val_end].copy()
            
            if len(val_df) > 0:  # Only add if validation set is non-empty
                splits.append((train_df, val_df))
        
        logger.info(
            "Walk-forward splits generated",
            n_splits=len(splits),
            min_train_size=min_train_size,
            test_size=test_size,
        )
        
        return splits
    
    @staticmethod
    def verify_no_leakage(
        train_df: pd.DataFrame,
        test_df: pd.DataFrame,
        timestamp_col: Optional[str] = None,
    ) -> bool:
        """
        Verify that no temporal leakage exists between train and test sets.
        
        Args:
            train_df: Training dataframe
            test_df: Test dataframe
            timestamp_col: Name of timestamp column (if not using datetime index)
        
        Returns:
            True if no leakage detected, False otherwise
        """
        if train_df.empty or test_df.empty:
            logger.warning("Cannot verify leakage on empty dataframe")
            return True
        
        # Get timestamps
        if isinstance(train_df.index, pd.DatetimeIndex):
            train_max_time = train_df.index.max()
            test_min_time = test_df.index.min()
        elif timestamp_col and timestamp_col in train_df.columns:
            train_max_time = train_df[timestamp_col].max()
            test_min_time = test_df[timestamp_col].min()
        else:
            logger.warning(
                "No datetime index or timestamp column found. Cannot verify leakage."
            )
            return True
        
        # Check for leakage
        has_leakage = train_max_time >= test_min_time
        
        if has_leakage:
            logger.error(
                "Temporal leakage detected!",
                train_max_time=str(train_max_time),
                test_min_time=str(test_min_time),
            )
        else:
            logger.info(
                "No temporal leakage detected",
                train_max_time=str(train_max_time),
                test_min_time=str(test_min_time),
            )
        
        return not has_leakage



