#pragma once

#include "ml/TechnicalIndicators.h"
#include "models/DataPoint.h"
#include <Eigen/Dense>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace ml {

/**
 * @brief Container for a feature matrix with column names and timestamps.
 */
struct FeatureMatrix {
  Eigen::MatrixXd data;
  std::vector<std::string> column_names;
  std::vector<std::chrono::system_clock::time_point> timestamps;

  /// Number of rows
  int rows() const { return static_cast<int>(data.rows()); }

  /// Number of feature columns
  int cols() const { return static_cast<int>(data.cols()); }

  /// Get column index by name, returns -1 if not found
  int column_index(const std::string &name) const;

  /// Get a column vector by name
  std::optional<Eigen::VectorXd> column(const std::string &name) const;
};

struct FeatureEngineerConfig {
  IndicatorConfig indicator_config;

  /// Lag periods for close_price and volume
  std::vector<int> lags = {1, 6, 24};

  /// Rolling window sizes for mean/std
  std::vector<int> rolling_windows = {6, 20};

  /**
   * Multiple time horizons for feature extraction.
   * Each horizon generates returns, lags, and rolling stats at that scale.
   * Values represent number of periods (e.g., {1, 6, 24, 48, 168}).
   * The default {1, 6, 24} targets short/medium/long-term patterns.
   */
  std::vector<int> horizons = {1, 6, 24};

  /// Whether to include temporal features (hour, day-of-week, month)
  bool include_temporal = true;
};

/**
 * @brief Builds ML-ready feature matrices from OHLCV data.
 *
 * Composes TechnicalIndicators with returns, lags, rolling statistics,
 * multi-horizon features, and temporal encodings.
 */
class FeatureEngineer {
public:
  explicit FeatureEngineer(
      const FeatureEngineerConfig &config = FeatureEngineerConfig{});

  /**
   * @brief Build features from a vector of CryptoPriceData.
   *
   * Converts Decimal prices to double, computes indicators and
   * engineered features, and removes rows containing NaN.
   */
  FeatureMatrix
  build_features(const std::vector<models::CryptoPriceData> &data) const;

private:
  FeatureEngineerConfig config_;

  /// Convert CryptoPriceData vector to Nx5 OHLCV double matrix
  static Eigen::MatrixXd
  to_ohlcv_matrix(const std::vector<models::CryptoPriceData> &data);

  /// Add return features at various horizons
  void add_returns(Eigen::MatrixXd &matrix, const Eigen::VectorXd &close,
                   std::vector<std::string> &col_names) const;

  /// Add lag features
  void add_lags(Eigen::MatrixXd &matrix, const Eigen::VectorXd &close,
                const Eigen::VectorXd &volume,
                std::vector<std::string> &col_names) const;

  /// Add rolling statistics
  void add_rolling_stats(Eigen::MatrixXd &matrix, const Eigen::VectorXd &close,
                         const Eigen::VectorXd &volume,
                         std::vector<std::string> &col_names) const;

  /// Add temporal features (hour, day-of-week, month)
  void add_temporal_features(
      Eigen::MatrixXd &matrix,
      const std::vector<std::chrono::system_clock::time_point> &timestamps,
      std::vector<std::string> &col_names) const;

  /// Add multi-horizon features (returns at different time scales)
  void add_horizon_features(Eigen::MatrixXd &matrix,
                            const Eigen::VectorXd &close,
                            const Eigen::VectorXd &volume,
                            std::vector<std::string> &col_names) const;

  /// Helper: append a column to a matrix
  static void append_column(Eigen::MatrixXd &matrix,
                            const Eigen::VectorXd &col);

  /// Helper: compute percentage change with a given period
  static Eigen::VectorXd pct_change(const Eigen::VectorXd &data, int period);

  /// Helper: rolling mean
  static Eigen::VectorXd rolling_mean(const Eigen::VectorXd &data, int window);

  /// Helper: rolling standard deviation
  static Eigen::VectorXd rolling_std(const Eigen::VectorXd &data, int window);
};

} // namespace ml
