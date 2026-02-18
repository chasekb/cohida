#include "ml/TechnicalIndicators.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ml {

// ------------------------------------------------------------------ helpers
static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

// ------------------------------------------------------------------ SMA
Eigen::VectorXd TechnicalIndicators::sma(const Eigen::VectorXd &data,
                                         int period) {
  const int n = static_cast<int>(data.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);
  if (period <= 0 || period > n)
    return result;

  double sum = 0.0;
  for (int i = 0; i < period; ++i)
    sum += data(i);
  result(period - 1) = sum / period;
  for (int i = period; i < n; ++i) {
    sum += data(i) - data(i - period);
    result(i) = sum / period;
  }
  return result;
}

// ------------------------------------------------------------------ EMA
Eigen::VectorXd TechnicalIndicators::ema(const Eigen::VectorXd &data,
                                         int period) {
  const int n = static_cast<int>(data.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);
  if (period <= 0 || period > n)
    return result;

  const double alpha = 2.0 / (period + 1.0);

  // Seed with SMA of first `period` values
  double seed = 0.0;
  for (int i = 0; i < period; ++i)
    seed += data(i);
  seed /= period;
  result(period - 1) = seed;

  for (int i = period; i < n; ++i) {
    result(i) = alpha * data(i) + (1.0 - alpha) * result(i - 1);
  }
  return result;
}

// ------------------------------------------------------------------ RSI
Eigen::VectorXd TechnicalIndicators::rsi(const Eigen::VectorXd &close,
                                         int period) {
  const int n = static_cast<int>(close.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);
  if (period <= 0 || period >= n)
    return result;

  // Compute deltas
  Eigen::VectorXd gain = Eigen::VectorXd::Zero(n);
  Eigen::VectorXd loss = Eigen::VectorXd::Zero(n);
  for (int i = 1; i < n; ++i) {
    double delta = close(i) - close(i - 1);
    gain(i) = std::max(delta, 0.0);
    loss(i) = std::max(-delta, 0.0);
  }

  // Wilder's smoothing (EMA with alpha = 1/period)
  const double alpha = 1.0 / period;
  Eigen::VectorXd avg_gain = Eigen::VectorXd::Constant(n, NaN);
  Eigen::VectorXd avg_loss = Eigen::VectorXd::Constant(n, NaN);

  // Seed
  double g_sum = 0.0, l_sum = 0.0;
  for (int i = 1; i <= period; ++i) {
    g_sum += gain(i);
    l_sum += loss(i);
  }
  avg_gain(period) = g_sum / period;
  avg_loss(period) = l_sum / period;

  for (int i = period + 1; i < n; ++i) {
    avg_gain(i) = alpha * gain(i) + (1.0 - alpha) * avg_gain(i - 1);
    avg_loss(i) = alpha * loss(i) + (1.0 - alpha) * avg_loss(i - 1);
  }

  for (int i = period; i < n; ++i) {
    if (avg_loss(i) == 0.0) {
      result(i) = 100.0;
    } else {
      double rs = avg_gain(i) / avg_loss(i);
      result(i) = 100.0 - (100.0 / (1.0 + rs));
    }
  }
  return result;
}

// ------------------------------------------------------------------ MACD
MACDResult TechnicalIndicators::macd(const Eigen::VectorXd &close, int fast,
                                     int slow, int signal) {
  auto ema_fast = ema(close, fast);
  auto ema_slow = ema(close, slow);

  const int n = static_cast<int>(close.size());
  Eigen::VectorXd macd_line = Eigen::VectorXd::Constant(n, NaN);
  for (int i = 0; i < n; ++i) {
    if (!std::isnan(ema_fast(i)) && !std::isnan(ema_slow(i)))
      macd_line(i) = ema_fast(i) - ema_slow(i);
  }

  // Signal line = EMA of MACD line (ignoring leading NaN)
  // Build a compact vector of valid MACD values, compute EMA, place back
  Eigen::VectorXd macd_signal = Eigen::VectorXd::Constant(n, NaN);
  Eigen::VectorXd macd_hist = Eigen::VectorXd::Constant(n, NaN);

  // Find first valid index
  int first_valid = -1;
  for (int i = 0; i < n; ++i) {
    if (!std::isnan(macd_line(i))) {
      first_valid = i;
      break;
    }
  }
  if (first_valid >= 0 && first_valid + signal <= n) {
    // Compute EMA of macd_line starting from first_valid
    const double alpha = 2.0 / (signal + 1.0);
    double seed = 0.0;
    for (int i = first_valid; i < first_valid + signal; ++i)
      seed += macd_line(i);
    seed /= signal;
    macd_signal(first_valid + signal - 1) = seed;

    for (int i = first_valid + signal; i < n; ++i) {
      if (!std::isnan(macd_line(i)))
        macd_signal(i) =
            alpha * macd_line(i) + (1.0 - alpha) * macd_signal(i - 1);
    }

    for (int i = 0; i < n; ++i) {
      if (!std::isnan(macd_line(i)) && !std::isnan(macd_signal(i)))
        macd_hist(i) = macd_line(i) - macd_signal(i);
    }
  }

  return {macd_line, macd_signal, macd_hist};
}

// ------------------------------------------------------------------ Bollinger
BollingerBandsResult
TechnicalIndicators::bollinger_bands(const Eigen::VectorXd &close, int period,
                                     double num_std) {
  const int n = static_cast<int>(close.size());
  Eigen::VectorXd middle = sma(close, period);
  Eigen::VectorXd upper = Eigen::VectorXd::Constant(n, NaN);
  Eigen::VectorXd lower = Eigen::VectorXd::Constant(n, NaN);

  for (int i = period - 1; i < n; ++i) {
    double sum2 = 0.0;
    for (int j = i - period + 1; j <= i; ++j) {
      double diff = close(j) - middle(i);
      sum2 += diff * diff;
    }
    double std_dev = std::sqrt(sum2 / period);
    upper(i) = middle(i) + num_std * std_dev;
    lower(i) = middle(i) - num_std * std_dev;
  }

  return {upper, middle, lower};
}

// ------------------------------------------------------------------ True Range
Eigen::VectorXd TechnicalIndicators::true_range(const Eigen::VectorXd &high,
                                                const Eigen::VectorXd &low,
                                                const Eigen::VectorXd &close) {
  const int n = static_cast<int>(high.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);

  for (int i = 1; i < n; ++i) {
    double hl = high(i) - low(i);
    double hc = std::abs(high(i) - close(i - 1));
    double lc = std::abs(low(i) - close(i - 1));
    result(i) = std::max({hl, hc, lc});
  }
  return result;
}

// ------------------------------------------------------------------ ATR
Eigen::VectorXd TechnicalIndicators::atr(const Eigen::VectorXd &high,
                                         const Eigen::VectorXd &low,
                                         const Eigen::VectorXd &close,
                                         int period) {
  auto tr = true_range(high, low, close);
  const int n = static_cast<int>(tr.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);
  if (period <= 0 || period >= n)
    return result;

  // Wilder's smoothing on TR
  const double alpha = 1.0 / period;
  double seed = 0.0;
  for (int i = 1; i <= period; ++i)
    seed += tr(i);
  seed /= period;
  result(period) = seed;

  for (int i = period + 1; i < n; ++i) {
    result(i) = alpha * tr(i) + (1.0 - alpha) * result(i - 1);
  }
  return result;
}

// ------------------------------------------------------------------ OBV
Eigen::VectorXd TechnicalIndicators::obv(const Eigen::VectorXd &close,
                                         const Eigen::VectorXd &volume) {
  const int n = static_cast<int>(close.size());
  Eigen::VectorXd result = Eigen::VectorXd::Zero(n);

  for (int i = 1; i < n; ++i) {
    double delta = close(i) - close(i - 1);
    double dir = (delta > 0.0) ? 1.0 : ((delta < 0.0) ? -1.0 : 0.0);
    result(i) = result(i - 1) + dir * volume(i);
  }
  return result;
}

// ------------------------------------------------------------------ VWAP
Eigen::VectorXd TechnicalIndicators::vwap(const Eigen::VectorXd &high,
                                          const Eigen::VectorXd &low,
                                          const Eigen::VectorXd &close,
                                          const Eigen::VectorXd &volume) {
  const int n = static_cast<int>(high.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);

  double cum_tp_vol = 0.0;
  double cum_vol = 0.0;
  for (int i = 0; i < n; ++i) {
    double tp = (high(i) + low(i) + close(i)) / 3.0;
    cum_tp_vol += tp * volume(i);
    cum_vol += volume(i);
    result(i) = (cum_vol > 0.0) ? (cum_tp_vol / cum_vol) : NaN;
  }
  return result;
}

// ------------------------------------------------------------------ build_all
Eigen::MatrixXd
TechnicalIndicators::build_all(const Eigen::MatrixXd &ohlcv,
                               const IndicatorConfig &config,
                               std::vector<std::string> &col_names) {

  // Columns: open(0), high(1), low(2), close(3), volume(4)
  const Eigen::VectorXd &open_col = ohlcv.col(0);
  const Eigen::VectorXd &high_col = ohlcv.col(1);
  const Eigen::VectorXd &low_col = ohlcv.col(2);
  const Eigen::VectorXd &close_col = ohlcv.col(3);
  const Eigen::VectorXd &volume_col = ohlcv.col(4);

  // Suppress unused variable warning
  (void)open_col;

  col_names = {"open", "high", "low", "close", "volume"};

  // Collect extra columns
  std::vector<Eigen::VectorXd> extras;

  // SMA
  for (int p : config.sma_periods) {
    extras.push_back(sma(close_col, p));
    col_names.push_back("sma_" + std::to_string(p));
  }

  // EMA
  for (int p : config.ema_periods) {
    extras.push_back(ema(close_col, p));
    col_names.push_back("ema_" + std::to_string(p));
  }

  // RSI
  extras.push_back(rsi(close_col, config.rsi_period));
  col_names.push_back("rsi_" + std::to_string(config.rsi_period));

  // MACD
  auto macd_result =
      macd(close_col, config.macd_fast, config.macd_slow, config.macd_signal);
  extras.push_back(macd_result.macd);
  col_names.push_back("macd");
  extras.push_back(macd_result.signal);
  col_names.push_back("macd_signal");
  extras.push_back(macd_result.histogram);
  col_names.push_back("macd_hist");

  // Bollinger Bands
  auto bb = bollinger_bands(close_col, config.bb_period, config.bb_std);
  extras.push_back(bb.upper);
  col_names.push_back("bb_upper");
  extras.push_back(bb.middle);
  col_names.push_back("bb_middle");
  extras.push_back(bb.lower);
  col_names.push_back("bb_lower");

  // ATR
  extras.push_back(atr(high_col, low_col, close_col, config.atr_period));
  col_names.push_back("atr_" + std::to_string(config.atr_period));

  // True Range
  extras.push_back(true_range(high_col, low_col, close_col));
  col_names.push_back("true_range");

  // OBV
  extras.push_back(obv(close_col, volume_col));
  col_names.push_back("obv");

  // VWAP
  extras.push_back(vwap(high_col, low_col, close_col, volume_col));
  col_names.push_back("vwap");

  // Concatenate
  const int n = static_cast<int>(ohlcv.rows());
  const int total_cols = 5 + static_cast<int>(extras.size());
  Eigen::MatrixXd result(n, total_cols);
  result.leftCols(5) = ohlcv;
  for (int c = 0; c < static_cast<int>(extras.size()); ++c) {
    result.col(5 + c) = extras[static_cast<size_t>(c)];
  }
  return result;
}

} // namespace ml
