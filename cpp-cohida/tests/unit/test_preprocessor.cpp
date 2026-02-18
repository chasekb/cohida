#include "ml/Preprocessor.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace ml;

class PreprocessorTest : public ::testing::Test {
protected:
  Eigen::MatrixXd X_;

  void SetUp() override {
    // 20 rows x 4 columns of synthetic data
    X_.resize(20, 4);
    for (int r = 0; r < 20; ++r) {
      X_(r, 0) = 100.0 + r * 2.0;              // linear
      X_(r, 1) = std::sin(r * 0.5) * 10.0;     // oscillating
      X_(r, 2) = 50.0 + r * 0.1 + (r % 3) * 5; // stepped
      X_(r, 3) = 1000.0 + r * 100.0;           // large scale
    }
  }
};

// --- Fit/Transform pattern ---
TEST_F(PreprocessorTest, NotFittedByDefault) {
  Preprocessor pp;
  EXPECT_FALSE(pp.is_fitted());
}

TEST_F(PreprocessorTest, FittedAfterFit) {
  Preprocessor pp;
  pp.fit(X_);
  EXPECT_TRUE(pp.is_fitted());
}

TEST_F(PreprocessorTest, FitTransformSameAsSequential) {
  PreprocessorConfig config;
  config.outlier_method = ""; // No outlier handling for exact comparison

  Preprocessor pp1(config);
  auto r1 = pp1.fit_transform(X_);

  Preprocessor pp2(config);
  pp2.fit(X_);
  auto r2 = pp2.transform(X_);

  EXPECT_EQ(r1.rows(), r2.rows());
  EXPECT_EQ(r1.cols(), r2.cols());
  for (int r = 0; r < r1.rows(); ++r) {
    for (int c = 0; c < r1.cols(); ++c) {
      EXPECT_NEAR(r1(r, c), r2(r, c), 1e-10);
    }
  }
}

// --- Standard scaler ---
TEST_F(PreprocessorTest, StandardScalerZeroMeanUnitVariance) {
  PreprocessorConfig config;
  config.scaler_type = "standard";
  config.outlier_method = "";

  Preprocessor pp(config);
  auto result = pp.fit_transform(X_);

  for (int c = 0; c < result.cols(); ++c) {
    double mean = result.col(c).mean();
    EXPECT_NEAR(mean, 0.0, 1e-10) << "Column " << c << " mean != 0";
  }
}

// --- Robust scaler ---
TEST_F(PreprocessorTest, RobustScalerOutputFinite) {
  PreprocessorConfig config;
  config.scaler_type = "robust";
  config.outlier_method = "";

  Preprocessor pp(config);
  auto result = pp.fit_transform(X_);

  for (int r = 0; r < result.rows(); ++r) {
    for (int c = 0; c < result.cols(); ++c) {
      EXPECT_TRUE(std::isfinite(result(r, c)));
    }
  }
}

// --- MinMax scaler ---
TEST_F(PreprocessorTest, MinMaxScalerBounded) {
  PreprocessorConfig config;
  config.scaler_type = "minmax";
  config.outlier_method = "";

  Preprocessor pp(config);
  auto result = pp.fit_transform(X_);

  for (int c = 0; c < result.cols(); ++c) {
    double col_min = result.col(c).minCoeff();
    double col_max = result.col(c).maxCoeff();
    EXPECT_GE(col_min, -1e-10) << "Column " << c << " min < 0";
    EXPECT_LE(col_max, 1.0 + 1e-10) << "Column " << c << " max > 1";
  }
}

// --- Inverse transform ---
TEST_F(PreprocessorTest, InverseTransformRecovery) {
  PreprocessorConfig config;
  config.scaler_type = "standard";
  config.outlier_method = "";

  Preprocessor pp(config);
  auto transformed = pp.fit_transform(X_);
  auto recovered = pp.inverse_transform(transformed);

  EXPECT_EQ(recovered.rows(), X_.rows());
  EXPECT_EQ(recovered.cols(), X_.cols());

  for (int r = 0; r < X_.rows(); ++r) {
    for (int c = 0; c < X_.cols(); ++c) {
      EXPECT_NEAR(recovered(r, c), X_(r, c), 1e-8)
          << "Mismatch at (" << r << ", " << c << ")";
    }
  }
}

// --- Shape preservation ---
TEST_F(PreprocessorTest, TransformPreservesShape) {
  Preprocessor pp;
  auto result = pp.fit_transform(X_);
  EXPECT_EQ(result.rows(), X_.rows());
  EXPECT_EQ(result.cols(), X_.cols());
}

// --- Feature count ---
TEST_F(PreprocessorTest, NFeaturesMatchesInput) {
  Preprocessor pp;
  pp.fit(X_);
  EXPECT_EQ(pp.n_features(), X_.cols());
}

// --- Get config ---
TEST_F(PreprocessorTest, GetConfigReturnsCorrectValues) {
  PreprocessorConfig config;
  config.scaler_type = "minmax";
  config.missing_strategy = "mean";
  config.outlier_method = "zscore";

  Preprocessor pp(config);
  auto cfg = pp.get_config();

  EXPECT_EQ(cfg["scaler_type"], "minmax");
  EXPECT_EQ(cfg["missing_strategy"], "mean");
  EXPECT_EQ(cfg["outlier_method"], "zscore");
}

// --- Excluded columns ---
TEST_F(PreprocessorTest, ExcludedColumnsNotScaled) {
  PreprocessorConfig config;
  config.scaler_type = "standard";
  config.outlier_method = "";
  config.exclude_from_scaling = {0, 2}; // Exclude columns 0 and 2

  Preprocessor pp(config);
  auto result = pp.fit_transform(X_);

  // Excluded columns should remain unchanged
  for (int r = 0; r < X_.rows(); ++r) {
    EXPECT_NEAR(result(r, 0), X_(r, 0), 1e-10);
    EXPECT_NEAR(result(r, 2), X_(r, 2), 1e-10);
  }
}
