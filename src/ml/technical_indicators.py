"""
Technical analysis indicator calculations for OHLCV time series.

All functions are implemented within the TechnicalIndicators class as
pure computations operating on pandas Series/DataFrame to ease testing
and reuse across the pipeline.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Optional

import pandas as pd


@dataclass
class IndicatorConfig:
    sma_periods: List[int] = (7, 14, 30, 50, 200)
    ema_periods: List[int] = (12, 26, 50)
    rsi_period: int = 14
    macd_fast: int = 12
    macd_slow: int = 26
    macd_signal: int = 9
    bb_period: int = 20
    bb_std: float = 2.0
    atr_period: int = 14


class TechnicalIndicators:
    """Collection of static methods for TA indicators."""

    @staticmethod
    def add_sma(df: pd.DataFrame, period: int, price_col: str = "close_price") -> pd.Series:
        return df[price_col].rolling(window=period, min_periods=period).mean().rename(f"sma_{period}")

    @staticmethod
    def add_ema(df: pd.DataFrame, period: int, price_col: str = "close_price") -> pd.Series:
        return df[price_col].ewm(span=period, adjust=False, min_periods=period).mean().rename(f"ema_{period}")

    @staticmethod
    def add_rsi(df: pd.DataFrame, period: int = 14, price_col: str = "close_price") -> pd.Series:
        delta = df[price_col].diff()
        gain = delta.clip(lower=0)
        loss = -delta.clip(upper=0)
        avg_gain = gain.ewm(alpha=1 / period, min_periods=period, adjust=False).mean()
        avg_loss = loss.ewm(alpha=1 / period, min_periods=period, adjust=False).mean()
        rs = avg_gain / (avg_loss.replace(0, pd.NA))
        rsi = 100 - (100 / (1 + rs))
        return rsi.rename(f"rsi_{period}")

    @staticmethod
    def add_macd(
        df: pd.DataFrame,
        fast: int = 12,
        slow: int = 26,
        signal: int = 9,
        price_col: str = "close_price",
    ) -> Dict[str, pd.Series]:
        ema_fast = df[price_col].ewm(span=fast, adjust=False, min_periods=fast).mean()
        ema_slow = df[price_col].ewm(span=slow, adjust=False, min_periods=slow).mean()
        macd = (ema_fast - ema_slow).rename("macd")
        macd_signal = macd.ewm(span=signal, adjust=False, min_periods=signal).mean().rename("macd_signal")
        macd_hist = (macd - macd_signal).rename("macd_hist")
        return {"macd": macd, "macd_signal": macd_signal, "macd_hist": macd_hist}

    @staticmethod
    def add_bollinger_bands(
        df: pd.DataFrame,
        period: int = 20,
        num_std: float = 2.0,
        price_col: str = "close_price",
    ) -> Dict[str, pd.Series]:
        ma = df[price_col].rolling(window=period, min_periods=period).mean()
        std = df[price_col].rolling(window=period, min_periods=period).std()
        upper = (ma + num_std * std).rename("bb_upper")
        lower = (ma - num_std * std).rename("bb_lower")
        ma = ma.rename("bb_middle")
        return {"bb_upper": upper, "bb_middle": ma, "bb_lower": lower}

    @staticmethod
    def add_true_range(df: pd.DataFrame) -> pd.Series:
        prev_close = df["close_price"].shift(1)
        high_low = df["high_price"] - df["low_price"]
        high_prev_close = (df["high_price"] - prev_close).abs()
        low_prev_close = (df["low_price"] - prev_close).abs()
        tr = pd.concat([high_low, high_prev_close, low_prev_close], axis=1).max(axis=1)
        return tr.rename("true_range")

    @staticmethod
    def add_atr(df: pd.DataFrame, period: int = 14) -> pd.Series:
        tr = TechnicalIndicators.add_true_range(df)
        atr = tr.ewm(alpha=1 / period, min_periods=period, adjust=False).mean()
        return atr.rename(f"atr_{period}")

    @staticmethod
    def add_obv(df: pd.DataFrame) -> pd.Series:
        direction = df["close_price"].diff().fillna(0).apply(lambda x: 1 if x > 0 else (-1 if x < 0 else 0))
        obv = (direction * df["volume"]).cumsum()
        return obv.rename("obv")

    @staticmethod
    def add_vwap(df: pd.DataFrame) -> pd.Series:
        typical_price = (df["high_price"] + df["low_price"] + df["close_price"]) / 3
        cum_vol = df["volume"].cumsum()
        cum_tp_vol = (typical_price * df["volume"]).cumsum()
        vwap = (cum_tp_vol / cum_vol).rename("vwap")
        return vwap

    @staticmethod
    def build_all(df: pd.DataFrame, config: Optional[IndicatorConfig] = None) -> pd.DataFrame:
        cfg = config or IndicatorConfig()
        out = df.copy()

        for p in cfg.sma_periods:
            out[f"sma_{p}"] = TechnicalIndicators.add_sma(out, p)

        for p in cfg.ema_periods:
            out[f"ema_{p}"] = TechnicalIndicators.add_ema(out, p)

        out[f"rsi_{cfg.rsi_period}"] = TechnicalIndicators.add_rsi(out, cfg.rsi_period)

        macd_cols = TechnicalIndicators.add_macd(out, cfg.macd_fast, cfg.macd_slow, cfg.macd_signal)
        for k, v in macd_cols.items():
            out[k] = v

        bb = TechnicalIndicators.add_bollinger_bands(out, cfg.bb_period, cfg.bb_std)
        for k, v in bb.items():
            out[k] = v

        out[f"atr_{cfg.atr_period}"] = TechnicalIndicators.add_atr(out, cfg.atr_period)
        out["true_range"] = TechnicalIndicators.add_true_range(out)
        out["obv"] = TechnicalIndicators.add_obv(out)
        out["vwap"] = TechnicalIndicators.add_vwap(out)

        return out


