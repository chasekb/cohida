#include "ml/DataSplitter.h"
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>

using namespace ml;
using namespace std::chrono;

class DataSplitterTest : public ::testing::Test {
protected:
  Eigen::MatrixXd X_;
  Eigen::VectorXd y_;
  std::vector<system_clock::time_point> timestamps_;

  void SetUp() override {
    // 100 samples x 5 features
    const int n = 100;
    X_.resize(n, 5);
    y_.resize(n);
    auto base_time = system_clock::now() - hours(n);

    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < 5; ++j) {
        X_(i, j) = i * 10.0 + j + std::sin(i * 0.3);
      }
      y_(i) = (i % 2 == 0) ? 1.0 : 0.0;
      timestamps_.push_back(base_time + hours(i));
    }
  }
};

// --- Basic split ---
TEST_F(DataSplitterTest, DefaultRatiosSplitCorrectly) {
  DataSplitter splitter; // 70/15/15
  auto split = splitter.split(X_, y_);

  EXPECT_EQ(split.train_size() + split.val_size() + split.test_size(),
            static_cast<int>(X_.rows()));
  EXPECT_GT(split.train_size(), 0);
  EXPECT_GT(split.val_size(), 0);
  EXPECT_GT(split.test_size(), 0);
}

TEST_F(DataSplitterTest, TrainIsLargestSplit) {
  DataSplitter splitter(0.7, 0.15, 0.15);
  auto split = splitter.split(X_, y_);

  EXPECT_GT(split.train_size(), split.val_size());
  EXPECT_GT(split.train_size(), split.test_size());
}

TEST_F(DataSplitterTest, FeatureColumnsPreserved) {
  DataSplitter splitter;
  auto split = splitter.split(X_, y_);

  EXPECT_EQ(split.train.cols(), X_.cols());
  EXPECT_EQ(split.val.cols(), X_.cols());
  EXPECT_EQ(split.test.cols(), X_.cols());
}

TEST_F(DataSplitterTest, TargetSizesMatchSplits) {
  DataSplitter splitter;
  auto split = splitter.split(X_, y_);

  EXPECT_EQ(split.train_target.size(), split.train_size());
  EXPECT_EQ(split.val_target.size(), split.val_size());
  EXPECT_EQ(split.test_target.size(), split.test_size());
}

// --- Custom ratios ---
TEST_F(DataSplitterTest, CustomRatios) {
  DataSplitter splitter(0.8, 0.1, 0.1);
  auto split = splitter.split(X_, y_);

  // Train should be around 80% of 100 = 80
  EXPECT_GE(split.train_size(), 75);
  EXPECT_LE(split.train_size(), 85);
}

// --- Temporal ordering preserved ---
TEST_F(DataSplitterTest, TemporalOrderPreserved) {
  DataSplitter splitter;
  auto split = splitter.split(X_, y_);

  // First feature of train's last row should be < first feature of val's
  // first row (since data is monotonically increasing)
  EXPECT_LT(split.train(split.train_size() - 1, 0), split.val(0, 0));

  if (split.val_size() > 0 && split.test_size() > 0) {
    EXPECT_LT(split.val(split.val_size() - 1, 0), split.test(0, 0));
  }
}

// --- Walk-forward validation ---
TEST_F(DataSplitterTest, WalkForwardSplitsCount) {
  DataSplitter splitter;
  auto folds = splitter.walk_forward_splits(X_, y_, 3);

  EXPECT_EQ(static_cast<int>(folds.size()), 3);
}

TEST_F(DataSplitterTest, WalkForwardExpandingTrainWindow) {
  DataSplitter splitter;
  auto folds = splitter.walk_forward_splits(X_, y_, 3);

  // Training set should grow with each fold
  for (size_t i = 1; i < folds.size(); ++i) {
    EXPECT_GE(folds[i].train_X.rows(), folds[i - 1].train_X.rows());
  }
}

TEST_F(DataSplitterTest, WalkForwardNoEmptyFolds) {
  DataSplitter splitter;
  auto folds = splitter.walk_forward_splits(X_, y_, 3);

  for (const auto &fold : folds) {
    EXPECT_GT(fold.train_X.rows(), 0);
    EXPECT_GT(fold.val_X.rows(), 0);
    EXPECT_EQ(fold.train_X.rows(), fold.train_y.size());
    EXPECT_EQ(fold.val_X.rows(), fold.val_y.size());
  }
}

// --- Leakage verification ---
TEST_F(DataSplitterTest, VerifyNoLeakageValid) {
  // train_end < test_start => no leakage
  EXPECT_TRUE(DataSplitter::verify_no_leakage(timestamps_, 49, 50));
}

TEST_F(DataSplitterTest, VerifyNoLeakageOverlap) {
  // train_end >= test_start => leakage
  EXPECT_FALSE(DataSplitter::verify_no_leakage(timestamps_, 50, 50));
}

// --- Summary ---
TEST_F(DataSplitterTest, SummaryNotEmpty) {
  DataSplitter splitter;
  auto split = splitter.split(X_, y_);
  std::string summary = split.summary();
  EXPECT_FALSE(summary.empty());
}
