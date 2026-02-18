#pragma once

#include <Eigen/Dense>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ml {

struct PreprocessorConfig {
  /// Scaling strategy: "standard", "robust", "minmax", or "" for none
  std::string scaler_type = "robust";

  /// Missing data handling: "drop", "ffill", "bfill", "interpolate", "mean"
  std::string missing_strategy = "ffill";

  /// Outlier detection: "iqr", "zscore", or "" for none
  std::string outlier_method = "iqr";

  /// Outlier treatment: "clip", "remove", "winsorize"
  std::string outlier_treatment = "clip";

  /// IQR multiplier for outlier detection
  double iqr_multiplier = 3.0;

  /// Z-score threshold for outlier detection
  double zscore_threshold = 3.0;

  /// Column indices to exclude from scaling (e.g., temporal features)
  std::vector<int> exclude_from_scaling;
};

/**
 * @brief Preprocesses feature matrices for ML training and inference.
 *
 * Handles scaling/normalization, missing data imputation, and
 * outlier detection/treatment. Follows the fit/transform pattern.
 */
class Preprocessor {
public:
  explicit Preprocessor(
      const PreprocessorConfig &config = PreprocessorConfig{});

  /// Fit on training data
  Preprocessor &fit(const Eigen::MatrixXd &X);

  /// Transform data using fitted parameters
  Eigen::MatrixXd transform(const Eigen::MatrixXd &X) const;

  /// Fit and transform in one step
  Eigen::MatrixXd fit_transform(const Eigen::MatrixXd &X);

  /// Inverse-transform scaled features back to original scale
  Eigen::MatrixXd inverse_transform(const Eigen::MatrixXd &X) const;

  /// Check if preprocessor has been fitted
  bool is_fitted() const { return fitted_; }

  /// Get config as key-value pairs for serialization
  std::map<std::string, std::string> get_config() const;

  /// Number of features at fit time
  int n_features() const { return n_features_; }

private:
  PreprocessorConfig config_;
  bool fitted_ = false;
  int n_features_ = 0;

  // Scaler parameters
  Eigen::VectorXd mean_;   // For standard scaler
  Eigen::VectorXd std_;    // For standard scaler
  Eigen::VectorXd median_; // For robust scaler
  Eigen::VectorXd iqr_;    // For robust scaler
  Eigen::VectorXd min_;    // For minmax scaler
  Eigen::VectorXd range_;  // For minmax scaler (max - min)

  // Outlier bounds per column
  Eigen::VectorXd lower_bounds_;
  Eigen::VectorXd upper_bounds_;

  /// Which columns to actually scale (all minus excluded)
  std::vector<int> scalable_cols_;

  void compute_scalable_cols(int total_cols);
  Eigen::MatrixXd handle_missing(const Eigen::MatrixXd &X) const;
  void fit_outlier_bounds(const Eigen::MatrixXd &X);
  Eigen::MatrixXd treat_outliers(const Eigen::MatrixXd &X) const;
  void fit_scaler(const Eigen::MatrixXd &X);
  Eigen::MatrixXd apply_scaler(const Eigen::MatrixXd &X) const;
  Eigen::MatrixXd apply_inverse_scaler(const Eigen::MatrixXd &X) const;
};

} // namespace ml
