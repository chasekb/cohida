#include "ml/FeatureEngineer.h"
#include "models/DataPoint.h"
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>

using namespace ml;
using namespace models;
using namespace std::chrono;

class FeatureEngineerTest : public ::testing::Test {
protected:
  std::vector<CryptoPriceData> sample_data_;

  void SetUp() override {
    // Generate 300 synthetic CryptoPriceData points to accommodate 200-period
    // SMA
    auto base_time = system_clock::now() - hours(300);
    Decimal base_price("100.00");

    for (int i = 0; i < 300; ++i) {
      auto ts = base_time + hours(i);
      // Simulate varying prices
      std::string price_str =
          std::to_string(100.0 + i * 0.5 + std::sin(i * 0.3) * 2.0);
      std::string high_str =
          std::to_string(100.0 + i * 0.5 + std::sin(i * 0.3) * 2.0 + 2.0);
      std::string low_str = std::to_string(
          std::max(0.1, 100.0 + i * 0.5 + std::sin(i * 0.3) * 2.0 - 2.0));
      std::string vol_str = std::to_string(1000.0 + (i % 10) * 100.0);

      sample_data_.emplace_back("BTC-USD", ts, Decimal(high_str),
                                Decimal(low_str), Decimal(price_str),
                                Decimal(price_str), Decimal(vol_str));
    }
  }
};

TEST_F(FeatureEngineerTest, BuildFeaturesReturnsNonEmptyMatrix) {
  FeatureEngineer fe;
  auto result = fe.build_features(sample_data_);

  EXPECT_GT(result.rows(), 0);
  EXPECT_GT(result.cols(), 5); // Should have more than OHLCV
}

TEST_F(FeatureEngineerTest, ColumnNamesMatchMatrixWidth) {
  FeatureEngineer fe;
  auto result = fe.build_features(sample_data_);

  EXPECT_EQ(static_cast<int>(result.column_names.size()), result.cols());
}

TEST_F(FeatureEngineerTest, TimestampsMatchMatrixHeight) {
  FeatureEngineer fe;
  auto result = fe.build_features(sample_data_);

  EXPECT_EQ(static_cast<int>(result.timestamps.size()), result.rows());
}

TEST_F(FeatureEngineerTest, ColumnIndexFindsExistingColumn) {
  FeatureEngineer fe;
  auto result = fe.build_features(sample_data_);

  // Close should always be in the feature matrix
  int idx = result.column_index("close");
  EXPECT_GE(idx, 0);
}

TEST_F(FeatureEngineerTest, ColumnIndexReturnsMinusOneForMissing) {
  FeatureEngineer fe;
  auto result = fe.build_features(sample_data_);

  int idx = result.column_index("nonexistent_column");
  EXPECT_EQ(idx, -1);
}

TEST_F(FeatureEngineerTest, ColumnByNameReturnsData) {
  FeatureEngineer fe;
  auto result = fe.build_features(sample_data_);

  auto col = result.column("close");
  EXPECT_TRUE(col.has_value());
  EXPECT_EQ(col->size(), result.rows());
}

TEST_F(FeatureEngineerTest, NoNaNsInCleanOutput) {
  FeatureEngineer fe;
  auto result = fe.build_features(sample_data_);

  // After NaN removal, no NaN should remain
  for (int r = 0; r < result.rows(); ++r) {
    for (int c = 0; c < result.cols(); ++c) {
      EXPECT_FALSE(std::isnan(result.data(r, c)))
          << "NaN at row " << r << " col " << c << " ("
          << result.column_names[c] << ")";
    }
  }
}

TEST_F(FeatureEngineerTest, CustomConfigReducesColumns) {
  FeatureEngineerConfig config;
  config.include_temporal = false;
  config.horizons = {1};
  config.lags = {1};
  config.rolling_windows = {6};

  FeatureEngineerConfig default_config;

  FeatureEngineer fe_custom(config);
  FeatureEngineer fe_default;

  auto result_custom = fe_custom.build_features(sample_data_);
  auto result_default = fe_default.build_features(sample_data_);

  // Custom config with fewer features should produce fewer columns
  EXPECT_LT(result_custom.cols(), result_default.cols());
}

TEST_F(FeatureEngineerTest, InsufficientDataThrows) {
  // Very small dataset
  std::vector<CryptoPriceData> tiny_data(sample_data_.begin(),
                                         sample_data_.begin() + 5);
  FeatureEngineer fe;
  // With only 5 points and indicators needing 200+ period,
  // the output should be empty after NaN removal
  auto result = fe.build_features(tiny_data);
  // Either throws or produces matrix with 0 rows
  EXPECT_EQ(result.rows(), 0);
}
