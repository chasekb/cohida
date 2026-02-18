#include "ml/FeatureEngineer.h"
#include "utils/Logger.h"
#include <boost/decimal/decimal128_t.hpp>
#include <cmath>
#include <ctime>
#include <limits>

namespace ml {

static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

// FeatureMatrix helpers
int FeatureMatrix::column_index(const std::string &name) const {
  for (int i = 0; i < static_cast<int>(column_names.size()); ++i) {
    if (column_names[static_cast<size_t>(i)] == name)
      return i;
  }
  return -1;
}

std::optional<Eigen::VectorXd>
FeatureMatrix::column(const std::string &name) const {
  int idx = column_index(name);
  if (idx < 0)
    return std::nullopt;
  return data.col(idx);
}

// --------------------------------------------------------------- Helpers
void FeatureEngineer::append_column(Eigen::MatrixXd &matrix,
                                    const Eigen::VectorXd &col) {
  const int rows = static_cast<int>(matrix.rows());
  const int cols = static_cast<int>(matrix.cols());
  matrix.conservativeResize(rows, cols + 1);
  matrix.col(cols) = col;
}

Eigen::VectorXd FeatureEngineer::pct_change(const Eigen::VectorXd &data,
                                            int period) {
  const int n = static_cast<int>(data.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);
  for (int i = period; i < n; ++i) {
    if (data(i - period) != 0.0)
      result(i) = (data(i) - data(i - period)) / data(i - period);
  }
  return result;
}

Eigen::VectorXd FeatureEngineer::rolling_mean(const Eigen::VectorXd &data,
                                              int window) {
  return TechnicalIndicators::sma(data, window);
}

Eigen::VectorXd FeatureEngineer::rolling_std(const Eigen::VectorXd &data,
                                             int window) {
  const int n = static_cast<int>(data.size());
  Eigen::VectorXd result = Eigen::VectorXd::Constant(n, NaN);
  if (window <= 0 || window > n)
    return result;

  for (int i = window - 1; i < n; ++i) {
    double mean = 0.0;
    for (int j = i - window + 1; j <= i; ++j)
      mean += data(j);
    mean /= window;

    double sum2 = 0.0;
    for (int j = i - window + 1; j <= i; ++j) {
      double diff = data(j) - mean;
      sum2 += diff * diff;
    }
    if (window > 1) {
      result(i) = std::sqrt(sum2 / (window - 1));
    } else {
      result(i) = 0.0;
    }
  }
  return result;
}

// --------------------------------------------------------------- Constructor
FeatureEngineer::FeatureEngineer(const FeatureEngineerConfig &config)
    : config_(config) {}

// --------------------------------------------------------------- OHLCV matrix
Eigen::MatrixXd FeatureEngineer::to_ohlcv_matrix(
    const std::vector<models::CryptoPriceData> &data) {
  const int n = static_cast<int>(data.size());
  Eigen::MatrixXd matrix(n, 5);
  for (int i = 0; i < n; ++i) {
    matrix(i, 0) = boost::decimal::to_float<models::Decimal, double>(
        data[static_cast<size_t>(i)].open_price);
    matrix(i, 1) = boost::decimal::to_float<models::Decimal, double>(
        data[static_cast<size_t>(i)].high_price);
    matrix(i, 2) = boost::decimal::to_float<models::Decimal, double>(
        data[static_cast<size_t>(i)].low_price);
    matrix(i, 3) = boost::decimal::to_float<models::Decimal, double>(
        data[static_cast<size_t>(i)].close_price);
    matrix(i, 4) = boost::decimal::to_float<models::Decimal, double>(
        data[static_cast<size_t>(i)].volume);
  }
  return matrix;
}

// ----------------------------------------------------- Returns
void FeatureEngineer::add_returns(Eigen::MatrixXd &matrix,
                                  const Eigen::VectorXd &close,
                                  std::vector<std::string> &col_names) const {
  for (int h : config_.horizons) {
    append_column(matrix, pct_change(close, h));
    col_names.push_back("ret_" + std::to_string(h));
  }
}

// ----------------------------------------------------- Lags
void FeatureEngineer::add_lags(Eigen::MatrixXd &matrix,
                               const Eigen::VectorXd &close,
                               const Eigen::VectorXd &volume,
                               std::vector<std::string> &col_names) const {
  const int n = static_cast<int>(close.size());
  for (int lag : config_.lags) {
    // Close lag
    Eigen::VectorXd close_lag = Eigen::VectorXd::Constant(n, NaN);
    for (int i = lag; i < n; ++i)
      close_lag(i) = close(i - lag);
    append_column(matrix, close_lag);
    col_names.push_back("close_lag_" + std::to_string(lag));

    // Volume lag
    Eigen::VectorXd vol_lag = Eigen::VectorXd::Constant(n, NaN);
    for (int i = lag; i < n; ++i)
      vol_lag(i) = volume(i - lag);
    append_column(matrix, vol_lag);
    col_names.push_back("vol_lag_" + std::to_string(lag));
  }
}

// ----------------------------------------------------- Rolling stats
void FeatureEngineer::add_rolling_stats(
    Eigen::MatrixXd &matrix, const Eigen::VectorXd &close,
    const Eigen::VectorXd &volume, std::vector<std::string> &col_names) const {
  for (int win : config_.rolling_windows) {
    append_column(matrix, rolling_mean(close, win));
    col_names.push_back("roll_mean_" + std::to_string(win));

    append_column(matrix, rolling_std(close, win));
    col_names.push_back("roll_std_" + std::to_string(win));

    append_column(matrix, rolling_mean(volume, win));
    col_names.push_back("roll_vol_mean_" + std::to_string(win));
  }
}

// ----------------------------------------------------- Temporal features
void FeatureEngineer::add_temporal_features(
    Eigen::MatrixXd &matrix,
    const std::vector<std::chrono::system_clock::time_point> &timestamps,
    std::vector<std::string> &col_names) const {
  if (!config_.include_temporal || timestamps.empty())
    return;

  const int n = static_cast<int>(timestamps.size());
  Eigen::VectorXd hour_col(n), dow_col(n), month_col(n);

  for (int i = 0; i < n; ++i) {
    auto time_t_val = std::chrono::system_clock::to_time_t(
        timestamps[static_cast<size_t>(i)]);
    std::tm tm_val = *std::gmtime(&time_t_val);
    hour_col(i) = tm_val.tm_hour;
    dow_col(i) = tm_val.tm_wday;
    month_col(i) = tm_val.tm_mon + 1;
  }

  append_column(matrix, hour_col);
  col_names.push_back("hour");
  append_column(matrix, dow_col);
  col_names.push_back("dayofweek");
  append_column(matrix, month_col);
  col_names.push_back("month");
}

// ----------------------------------------------------- Multi-horizon features
void FeatureEngineer::add_horizon_features(
    Eigen::MatrixXd &matrix, const Eigen::VectorXd &close,
    const Eigen::VectorXd &volume, std::vector<std::string> &col_names) const {
  for (int h : config_.horizons) {
    // Rolling mean/std at this horizon
    append_column(matrix, rolling_mean(close, h));
    col_names.push_back("hz_roll_mean_" + std::to_string(h));

    append_column(matrix, rolling_std(close, h));
    col_names.push_back("hz_roll_std_" + std::to_string(h));

    // Volume rolling mean at this horizon
    append_column(matrix, rolling_mean(volume, h));
    col_names.push_back("hz_vol_mean_" + std::to_string(h));

    // Momentum at this horizon (close - close_lag)
    const int n = static_cast<int>(close.size());
    Eigen::VectorXd momentum = Eigen::VectorXd::Constant(n, NaN);
    for (int i = h; i < n; ++i) {
      momentum(i) = close(i) - close(i - h);
    }
    append_column(matrix, momentum);
    col_names.push_back("hz_momentum_" + std::to_string(h));

    // Volatility ratio: rolling_std / rolling_mean
    auto rm = rolling_mean(close, h);
    auto rs = rolling_std(close, h);
    Eigen::VectorXd vol_ratio = Eigen::VectorXd::Constant(n, NaN);
    for (int i = 0; i < n; ++i) {
      if (!std::isnan(rm(i)) && rm(i) != 0.0 && !std::isnan(rs(i)))
        vol_ratio(i) = rs(i) / rm(i);
    }
    append_column(matrix, vol_ratio);
    col_names.push_back("hz_vol_ratio_" + std::to_string(h));
  }
}

// ---------------------------------------------------------------
// build_features
FeatureMatrix FeatureEngineer::build_features(
    const std::vector<models::CryptoPriceData> &data) const {

  FeatureMatrix result;
  if (data.empty())
    return result;

  // Extract timestamps
  result.timestamps.reserve(data.size());
  for (const auto &dp : data)
    result.timestamps.push_back(dp.timestamp);

  // Convert to OHLCV matrix
  Eigen::MatrixXd ohlcv = to_ohlcv_matrix(data);

  // Build technical indicators
  result.data = TechnicalIndicators::build_all(ohlcv, config_.indicator_config,
                                               result.column_names);

  // Get close and volume for further features
  Eigen::VectorXd close_col = ohlcv.col(3);
  Eigen::VectorXd volume_col = ohlcv.col(4);

  // Add returns
  add_returns(result.data, close_col, result.column_names);

  // Add lags
  add_lags(result.data, close_col, volume_col, result.column_names);

  // Add rolling stats
  add_rolling_stats(result.data, close_col, volume_col, result.column_names);

  // Add multi-horizon features
  add_horizon_features(result.data, close_col, volume_col, result.column_names);

  // Add temporal features
  add_temporal_features(result.data, result.timestamps, result.column_names);

  // Remove rows with NaNs
  int n_rows = result.rows();
  std::vector<int> valid_indices;
  for (int i = 0; i < n_rows; ++i) {
    if (result.data.row(i).array().isFinite().all()) {
      valid_indices.push_back(i);
    }
  }

  if (valid_indices.size() < static_cast<size_t>(n_rows)) {
    Eigen::MatrixXd filtered_data(static_cast<int>(valid_indices.size()),
                                  result.cols());
    std::vector<std::chrono::system_clock::time_point> filtered_timestamps;
    for (size_t i = 0; i < valid_indices.size(); ++i) {
      filtered_data.row(static_cast<int>(i)) =
          result.data.row(valid_indices[i]);
      filtered_timestamps.push_back(
          result.timestamps[static_cast<size_t>(valid_indices[i])]);
    }
    result.data = filtered_data;
    result.timestamps = filtered_timestamps;
    LOG_INFO("Removed {} rows containing NaNs. Remaining: {}",
             n_rows - static_cast<int>(valid_indices.size()), result.rows());
  }

  LOG_INFO("Feature matrix built: {} rows x {} cols", result.rows(),
           result.cols());

  return result;
}

} // namespace ml
