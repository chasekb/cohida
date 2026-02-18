#include "ml/DataSplitter.h"
#include "utils/Logger.h"
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace ml {

// --------------------------------------------------------------- DataSplit
std::string DataSplit::summary() const {
  int total = train_size() + val_size() + test_size();
  std::ostringstream oss;
  oss << "Split summary:\n"
      << "  Train: " << train_size() << " (" << (100.0 * train_size() / total)
      << "%)\n"
      << "  Val:   " << val_size() << " (" << (100.0 * val_size() / total)
      << "%)\n"
      << "  Test:  " << test_size() << " (" << (100.0 * test_size() / total)
      << "%)\n"
      << "  Total: " << total;
  return oss.str();
}

// --------------------------------------------------------------- DataSplitter
DataSplitter::DataSplitter(double train_ratio, double val_ratio,
                           double test_ratio)
    : train_ratio_(train_ratio), val_ratio_(val_ratio),
      test_ratio_(test_ratio) {
  if (std::abs(train_ratio + val_ratio + test_ratio - 1.0) > 1e-6)
    throw std::invalid_argument("Split ratios must sum to 1.0");
  if (train_ratio <= 0 || val_ratio <= 0 || test_ratio <= 0)
    throw std::invalid_argument("All split ratios must be positive");
}

DataSplit DataSplitter::split(const Eigen::MatrixXd &X,
                              const Eigen::VectorXd &y) const {
  const int n = static_cast<int>(X.rows());
  if (n == 0)
    throw std::invalid_argument("Cannot split empty data");
  if (n < 10)
    throw std::invalid_argument(
        "Data too small to split (need at least 10 rows)");
  if (X.rows() != y.size())
    throw std::invalid_argument("X and y must have same number of rows");

  int train_end = static_cast<int>(n * train_ratio_);
  int val_end = train_end + static_cast<int>(n * val_ratio_);

  // Ensure non-empty partitions
  if (train_end < 1)
    train_end = 1;
  if (val_end <= train_end)
    val_end = train_end + 1;
  if (val_end >= n)
    val_end = n - 1;

  DataSplit result;
  result.train = X.middleRows(0, train_end);
  result.val = X.middleRows(train_end, val_end - train_end);
  result.test = X.middleRows(val_end, n - val_end);

  result.train_target = y.segment(0, train_end);
  result.val_target = y.segment(train_end, val_end - train_end);
  result.test_target = y.segment(val_end, n - val_end);

  LOG_INFO("Data split: train={}, val={}, test={}", result.train_size(),
           result.val_size(), result.test_size());

  return result;
}

std::vector<DataSplitter::WalkForwardFold>
DataSplitter::walk_forward_splits(const Eigen::MatrixXd &X,
                                  const Eigen::VectorXd &y, int n_splits,
                                  std::optional<int> min_train_size) const {

  const int n = static_cast<int>(X.rows());
  if (n == 0)
    throw std::invalid_argument("Cannot split empty data");
  if (n_splits < 2)
    throw std::invalid_argument("n_splits must be at least 2");

  int min_train = min_train_size.value_or(static_cast<int>(n * 0.3));
  if (min_train >= n)
    throw std::invalid_argument("min_train_size must be less than data size");

  int remaining = n - min_train;
  int test_size = remaining / n_splits;
  if (test_size < 1)
    throw std::invalid_argument("Not enough data for requested splits");

  std::vector<WalkForwardFold> folds;
  for (int i = 0; i < n_splits; ++i) {
    int train_end = min_train + i * test_size;
    int val_end = std::min(train_end + test_size, n);

    if (val_end <= train_end)
      break;

    WalkForwardFold fold;
    fold.train_X = X.middleRows(0, train_end);
    fold.train_y = y.segment(0, train_end);
    fold.val_X = X.middleRows(train_end, val_end - train_end);
    fold.val_y = y.segment(train_end, val_end - train_end);
    folds.push_back(std::move(fold));
  }

  LOG_INFO("Walk-forward splits generated: {} folds, min_train={}",
           static_cast<int>(folds.size()), min_train);
  return folds;
}

bool DataSplitter::verify_no_leakage(
    const std::vector<std::chrono::system_clock::time_point> &timestamps,
    int train_end, int test_start) {

  if (timestamps.empty() || train_end < 0 || test_start < 0)
    return true;
  if (train_end >= static_cast<int>(timestamps.size()) ||
      test_start >= static_cast<int>(timestamps.size()))
    return true;

  bool has_leakage = timestamps[static_cast<size_t>(train_end)] >=
                     timestamps[static_cast<size_t>(test_start)];
  if (has_leakage) {
    LOG_ERROR("Temporal leakage detected!");
  }
  return !has_leakage;
}

} // namespace ml
