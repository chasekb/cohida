#pragma once

#include <Eigen/Dense>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ml {

/**
 * @brief Container for train/val/test data splits.
 */
struct DataSplit {
  Eigen::MatrixXd train;
  Eigen::MatrixXd val;
  Eigen::MatrixXd test;

  Eigen::VectorXd train_target;
  Eigen::VectorXd val_target;
  Eigen::VectorXd test_target;

  int train_size() const { return static_cast<int>(train.rows()); }
  int val_size() const { return static_cast<int>(val.rows()); }
  int test_size() const { return static_cast<int>(test.rows()); }

  std::string summary() const;
};

/**
 * @brief Time-series aware data splitting for ML training.
 *
 * Ensures proper temporal ordering to prevent lookahead bias.
 * Supports both fixed ratio splits and walk-forward validation.
 */
class DataSplitter {
public:
  DataSplitter(double train_ratio = 0.7, double val_ratio = 0.15,
               double test_ratio = 0.15);

  /**
   * @brief Split features and target into train/val/test sets.
   *
   * Data is split in temporal order (no shuffling).
   *
   * @param X  Feature matrix (rows ordered by time)
   * @param y  Target vector
   * @return   DataSplit containing partitioned data
   */
  DataSplit split(const Eigen::MatrixXd &X, const Eigen::VectorXd &y) const;

  /**
   * @brief Generate walk-forward validation splits.
   *
   * Expanding training window with fixed-size validation window.
   *
   * @param X  Feature matrix
   * @param y  Target vector
   * @param n_splits  Number of CV folds
   * @param min_train_size  Minimum training set size (default: 30%)
   * @return Vector of (train_X, train_y, val_X, val_y) tuples
   */
  struct WalkForwardFold {
    Eigen::MatrixXd train_X;
    Eigen::VectorXd train_y;
    Eigen::MatrixXd val_X;
    Eigen::VectorXd val_y;
  };

  std::vector<WalkForwardFold>
  walk_forward_splits(const Eigen::MatrixXd &X, const Eigen::VectorXd &y,
                      int n_splits = 5,
                      std::optional<int> min_train_size = std::nullopt) const;

  /**
   * @brief Verify no temporal leakage between train and test sets.
   *
   * Checks that timestamp indices are strictly non-overlapping.
   *
   * @param timestamps  Full timestamp vector
   * @param train_end   Index of last training sample
   * @param test_start  Index of first test sample
   * @return true if no leakage detected
   */
  static bool verify_no_leakage(
      const std::vector<std::chrono::system_clock::time_point> &timestamps,
      int train_end, int test_start);

private:
  double train_ratio_;
  double val_ratio_;
  double test_ratio_;
};

} // namespace ml
