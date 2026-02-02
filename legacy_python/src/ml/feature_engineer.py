"""
Feature engineering utilities building ML-ready datasets from OHLCV.

This module composes calculations from TechnicalIndicators and provides
helpers for lag features, rolling statistics, and temporal encodings.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

import pandas as pd

from .technical_indicators import TechnicalIndicators, IndicatorConfig


@dataclass
class FeatureEngineerConfig:
    indicator_config: IndicatorConfig = IndicatorConfig()
    lags: List[int] = (1, 6, 24)
    rolling_windows: List[int] = (6, 20)


class FeatureEngineer:
    """Build feature matrices from OHLCV DataFrames."""

    def __init__(self, config: Optional[FeatureEngineerConfig] = None):
        self.config = config or FeatureEngineerConfig()

    def build_features(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Build features given an OHLCV dataframe with columns:
        ['timestamp','open_price','high_price','low_price','close_price','volume']
        The index may be a datetime index or 'timestamp' column will be used.
        """
        if df.empty:
            return df.copy()

        work = df.copy()
        if 'timestamp' in work.columns and not isinstance(work.index, pd.DatetimeIndex):
            try:
                work = work.set_index(pd.to_datetime(work['timestamp']))
            except Exception:
                # Keep original index if conversion fails
                pass

        # Indicators
        work = TechnicalIndicators.build_all(work, self.config.indicator_config)

        # Returns
        work['ret_1'] = work['close_price'].pct_change(1)
        work['ret_6'] = work['close_price'].pct_change(6)
        work['ret_24'] = work['close_price'].pct_change(24)

        # Lags
        for lag in self.config.lags:
            work[f'close_lag_{lag}'] = work['close_price'].shift(lag)
            work[f'vol_lag_{lag}'] = work['volume'].shift(lag)

        # Rolling statistics
        for win in self.config.rolling_windows:
            work[f'roll_mean_{win}'] = work['close_price'].rolling(win, min_periods=win).mean()
            work[f'roll_std_{win}'] = work['close_price'].rolling(win, min_periods=win).std()
            work[f'roll_vol_mean_{win}'] = work['volume'].rolling(win, min_periods=win).mean()

        # Temporal features (if datetime index available)
        if isinstance(work.index, pd.DatetimeIndex):
            work['hour'] = work.index.hour
            work['dayofweek'] = work.index.dayofweek
            work['month'] = work.index.month

        return work


