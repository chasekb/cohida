#pragma once

#include <Eigen/Dense>
#include <map>
#include <string>
#include <vector>

namespace ml {

struct IndicatorConfig {
  std::vector<int> sma_periods = {7, 14, 30, 50, 200};
  std::vector<int> ema_periods = {12, 26, 50};
  int rsi_period = 14;
  int macd_fast = 12;
  int macd_slow = 26;
  int macd_signal = 9;
  int bb_period = 20;
  double bb_std = 2.0;
  int atr_period = 14;
};

struct MACDResult {
  Eigen::VectorXd macd;
  Eigen::VectorXd signal;
  Eigen::VectorXd histogram;
};

struct BollingerBandsResult {
  Eigen::VectorXd upper;
  Eigen::VectorXd middle;
  Eigen::VectorXd lower;
};

/**
 * @brief Collection of technical analysis indicator calculations for OHLCV
 * data.
 *
 * All functions operate on Eigen::VectorXd columns, mirroring the
 * pandas-based Python implementation.
 */
class TechnicalIndicators {
public:
  /// Simple Moving Average
  static Eigen::VectorXd sma(const Eigen::VectorXd &data, int period);

  /// Exponential Moving Average
  static Eigen::VectorXd ema(const Eigen::VectorXd &data, int period);

  /// Relative Strength Index
  static Eigen::VectorXd rsi(const Eigen::VectorXd &close, int period = 14);

  /// MACD (macd line, signal line, histogram)
  static MACDResult macd(const Eigen::VectorXd &close, int fast = 12,
                         int slow = 26, int signal = 9);

  /// Bollinger Bands (upper, middle, lower)
  static BollingerBandsResult bollinger_bands(const Eigen::VectorXd &close,
                                              int period = 20,
                                              double num_std = 2.0);

  /// True Range
  static Eigen::VectorXd true_range(const Eigen::VectorXd &high,
                                    const Eigen::VectorXd &low,
                                    const Eigen::VectorXd &close);

  /// Average True Range
  static Eigen::VectorXd atr(const Eigen::VectorXd &high,
                             const Eigen::VectorXd &low,
                             const Eigen::VectorXd &close, int period = 14);

  /// On-Balance Volume
  static Eigen::VectorXd obv(const Eigen::VectorXd &close,
                             const Eigen::VectorXd &volume);

  /// Volume Weighted Average Price
  static Eigen::VectorXd vwap(const Eigen::VectorXd &high,
                              const Eigen::VectorXd &low,
                              const Eigen::VectorXd &close,
                              const Eigen::VectorXd &volume);

  /**
   * @brief Build all indicators and append columns to an OHLCV matrix.
   *
   * The input matrix must have columns in order:
   *   [open, high, low, close, volume]
   *
   * @param ohlcv  Nx5 matrix
   * @param config Indicator configuration
   * @param[out] col_names  Names for the appended columns
   * @return Concatenated matrix [ohlcv | indicator_columns]
   */
  static Eigen::MatrixXd build_all(const Eigen::MatrixXd &ohlcv,
                                   const IndicatorConfig &config,
                                   std::vector<std::string> &col_names);
};

} // namespace ml
