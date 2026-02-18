#include "ml/Preprocessor.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace ml {

static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

Preprocessor::Preprocessor(const PreprocessorConfig &config)
    : config_(config) {}

void Preprocessor::compute_scalable_cols(int total_cols) {
  scalable_cols_.clear();
  for (int c = 0; c < total_cols; ++c) {
    bool excluded = false;
    for (int ex : config_.exclude_from_scaling) {
      if (c == ex) {
        excluded = true;
        break;
      }
    }
    if (!excluded)
      scalable_cols_.push_back(c);
  }
}

// -------------------------------------------------------- Missing data
Eigen::MatrixXd Preprocessor::handle_missing(const Eigen::MatrixXd &X) const {
  Eigen::MatrixXd result = X;
  const int rows = static_cast<int>(result.rows());
  const int cols = static_cast<int>(result.cols());

  if (config_.missing_strategy == "ffill") {
    for (int c = 0; c < cols; ++c) {
      for (int r = 1; r < rows; ++r) {
        if (std::isnan(result(r, c)))
          result(r, c) = result(r - 1, c);
      }
    }
  } else if (config_.missing_strategy == "bfill") {
    for (int c = 0; c < cols; ++c) {
      for (int r = rows - 2; r >= 0; --r) {
        if (std::isnan(result(r, c)))
          result(r, c) = result(r + 1, c);
      }
    }
  } else if (config_.missing_strategy == "mean") {
    for (int c = 0; c < cols; ++c) {
      double sum = 0.0;
      int count = 0;
      for (int r = 0; r < rows; ++r) {
        if (!std::isnan(result(r, c))) {
          sum += result(r, c);
          ++count;
        }
      }
      if (count > 0) {
        double mean = sum / count;
        for (int r = 0; r < rows; ++r) {
          if (std::isnan(result(r, c)))
            result(r, c) = mean;
        }
      }
    }
  } else if (config_.missing_strategy == "interpolate") {
    for (int c = 0; c < cols; ++c) {
      // Linear interpolation
      int last_valid = -1;
      for (int r = 0; r < rows; ++r) {
        if (!std::isnan(result(r, c))) {
          if (last_valid >= 0 && r - last_valid > 1) {
            double start_val = result(last_valid, c);
            double end_val = result(r, c);
            int gap = r - last_valid;
            for (int g = 1; g < gap; ++g) {
              result(last_valid + g, c) =
                  start_val + (end_val - start_val) * g / gap;
            }
          }
          last_valid = r;
        }
      }
    }
  }
  // "drop" is handled by the caller removing NaN rows

  // Final pass: if still NaN, mark for drop
  if (config_.missing_strategy == "drop") {
    // Caller handles row removal
  }

  return result;
}

// -------------------------------------------------------- Outlier bounds
void Preprocessor::fit_outlier_bounds(const Eigen::MatrixXd &X) {
  const int cols = static_cast<int>(X.cols());
  lower_bounds_ =
      Eigen::VectorXd::Constant(cols, -std::numeric_limits<double>::infinity());
  upper_bounds_ =
      Eigen::VectorXd::Constant(cols, std::numeric_limits<double>::infinity());

  for (int c : scalable_cols_) {
    // Gather non-NaN values
    std::vector<double> vals;
    for (int r = 0; r < static_cast<int>(X.rows()); ++r) {
      if (!std::isnan(X(r, c)))
        vals.push_back(X(r, c));
    }
    if (vals.empty())
      continue;

    if (config_.outlier_method == "iqr") {
      std::sort(vals.begin(), vals.end());
      auto n = vals.size();
      double q1 = vals[n / 4];
      double q3 = vals[3 * n / 4];
      double iqr = q3 - q1;
      lower_bounds_(c) = q1 - config_.iqr_multiplier * iqr;
      upper_bounds_(c) = q3 + config_.iqr_multiplier * iqr;
    } else if (config_.outlier_method == "zscore") {
      double sum = 0.0;
      for (double v : vals)
        sum += v;
      double mean = sum / static_cast<double>(vals.size());
      double sum2 = 0.0;
      for (double v : vals)
        sum2 += (v - mean) * (v - mean);
      double std_dev = std::sqrt(sum2 / static_cast<double>(vals.size()));
      lower_bounds_(c) = mean - config_.zscore_threshold * std_dev;
      upper_bounds_(c) = mean + config_.zscore_threshold * std_dev;
    }
  }
  LOG_INFO("Outlier bounds fitted (method={})", config_.outlier_method);
}

Eigen::MatrixXd Preprocessor::treat_outliers(const Eigen::MatrixXd &X) const {
  Eigen::MatrixXd result = X;
  for (int c : scalable_cols_) {
    if (config_.outlier_treatment == "clip" ||
        config_.outlier_treatment == "winsorize") {
      for (int r = 0; r < static_cast<int>(result.rows()); ++r) {
        if (!std::isnan(result(r, c))) {
          result(r, c) =
              std::clamp(result(r, c), lower_bounds_(c), upper_bounds_(c));
        }
      }
    }
    // "remove" handled by caller filtering rows
  }
  return result;
}

// -------------------------------------------------------- Scaler
void Preprocessor::fit_scaler(const Eigen::MatrixXd &X) {
  const int cols = static_cast<int>(X.cols());
  const int rows = static_cast<int>(X.rows());

  if (config_.scaler_type == "standard") {
    mean_ = Eigen::VectorXd::Zero(cols);
    std_ = Eigen::VectorXd::Ones(cols);
    for (int c : scalable_cols_) {
      double sum = 0.0;
      int cnt = 0;
      for (int r = 0; r < rows; ++r) {
        if (!std::isnan(X(r, c))) {
          sum += X(r, c);
          ++cnt;
        }
      }
      if (cnt == 0)
        continue;
      mean_(c) = sum / cnt;
      double sum2 = 0.0;
      for (int r = 0; r < rows; ++r) {
        if (!std::isnan(X(r, c))) {
          double d = X(r, c) - mean_(c);
          sum2 += d * d;
        }
      }
      std_(c) = (cnt > 1) ? std::sqrt(sum2 / (cnt - 1)) : 1.0;
      if (std_(c) < 1e-12)
        std_(c) = 1.0;
    }
  } else if (config_.scaler_type == "robust") {
    median_ = Eigen::VectorXd::Zero(cols);
    iqr_ = Eigen::VectorXd::Ones(cols);
    for (int c : scalable_cols_) {
      std::vector<double> vals;
      for (int r = 0; r < rows; ++r) {
        if (!std::isnan(X(r, c)))
          vals.push_back(X(r, c));
      }
      if (vals.empty())
        continue;
      std::sort(vals.begin(), vals.end());
      auto n = vals.size();
      median_(c) = vals[n / 2];
      double q1 = vals[n / 4];
      double q3 = vals[3 * n / 4];
      iqr_(c) = q3 - q1;
      if (iqr_(c) < 1e-12)
        iqr_(c) = 1.0;
    }
  } else if (config_.scaler_type == "minmax") {
    min_ = Eigen::VectorXd::Zero(cols);
    range_ = Eigen::VectorXd::Ones(cols);
    for (int c : scalable_cols_) {
      double lo = std::numeric_limits<double>::max();
      double hi = std::numeric_limits<double>::lowest();
      for (int r = 0; r < rows; ++r) {
        if (!std::isnan(X(r, c))) {
          lo = std::min(lo, X(r, c));
          hi = std::max(hi, X(r, c));
        }
      }
      min_(c) = lo;
      range_(c) = (hi - lo > 1e-12) ? (hi - lo) : 1.0;
    }
  }
  LOG_INFO("Scaler fitted (type={}, features={})", config_.scaler_type,
           static_cast<int>(scalable_cols_.size()));
}

Eigen::MatrixXd Preprocessor::apply_scaler(const Eigen::MatrixXd &X) const {
  Eigen::MatrixXd result = X;
  for (int c : scalable_cols_) {
    for (int r = 0; r < static_cast<int>(result.rows()); ++r) {
      if (std::isnan(result(r, c)))
        continue;
      if (config_.scaler_type == "standard") {
        result(r, c) = (result(r, c) - mean_(c)) / std_(c);
      } else if (config_.scaler_type == "robust") {
        result(r, c) = (result(r, c) - median_(c)) / iqr_(c);
      } else if (config_.scaler_type == "minmax") {
        result(r, c) = (result(r, c) - min_(c)) / range_(c);
      }
    }
  }
  return result;
}

Eigen::MatrixXd
Preprocessor::apply_inverse_scaler(const Eigen::MatrixXd &X) const {
  Eigen::MatrixXd result = X;
  for (int c : scalable_cols_) {
    for (int r = 0; r < static_cast<int>(result.rows()); ++r) {
      if (std::isnan(result(r, c)))
        continue;
      if (config_.scaler_type == "standard") {
        result(r, c) = result(r, c) * std_(c) + mean_(c);
      } else if (config_.scaler_type == "robust") {
        result(r, c) = result(r, c) * iqr_(c) + median_(c);
      } else if (config_.scaler_type == "minmax") {
        result(r, c) = result(r, c) * range_(c) + min_(c);
      }
    }
  }
  return result;
}

// -------------------------------------------------------- Public API
Preprocessor &Preprocessor::fit(const Eigen::MatrixXd &X) {
  if (X.rows() == 0)
    throw std::invalid_argument("Cannot fit on empty data");

  n_features_ = static_cast<int>(X.cols());
  compute_scalable_cols(n_features_);

  // Handle missing data
  Eigen::MatrixXd clean = handle_missing(X);

  // Fit outlier bounds
  if (!config_.outlier_method.empty())
    fit_outlier_bounds(clean);

  // Fit scaler
  if (!config_.scaler_type.empty())
    fit_scaler(clean);

  fitted_ = true;
  return *this;
}

Eigen::MatrixXd Preprocessor::transform(const Eigen::MatrixXd &X) const {
  if (!fitted_)
    throw std::runtime_error("Preprocessor must be fitted before transform");
  if (X.rows() == 0)
    return X;

  Eigen::MatrixXd result = handle_missing(X);

  if (!config_.outlier_method.empty())
    result = treat_outliers(result);

  if (!config_.scaler_type.empty())
    result = apply_scaler(result);

  return result;
}

Eigen::MatrixXd Preprocessor::fit_transform(const Eigen::MatrixXd &X) {
  fit(X);
  return transform(X);
}

Eigen::MatrixXd
Preprocessor::inverse_transform(const Eigen::MatrixXd &X) const {
  if (!fitted_ || config_.scaler_type.empty())
    return X;
  return apply_inverse_scaler(X);
}

std::map<std::string, std::string> Preprocessor::get_config() const {
  return {
      {"scaler_type", config_.scaler_type},
      {"missing_strategy", config_.missing_strategy},
      {"outlier_method", config_.outlier_method},
      {"outlier_treatment", config_.outlier_treatment},
      {"iqr_multiplier", std::to_string(config_.iqr_multiplier)},
      {"zscore_threshold", std::to_string(config_.zscore_threshold)},
      {"n_features", std::to_string(n_features_)},
  };
}

} // namespace ml
