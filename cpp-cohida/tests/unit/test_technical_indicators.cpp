#include "ml/TechnicalIndicators.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace ml;

class TechnicalIndicatorsTest : public ::testing::Test {
protected:
  Eigen::VectorXd close_;
  Eigen::VectorXd high_;
  Eigen::VectorXd low_;
  Eigen::VectorXd volume_;

  void SetUp() override {
    // Synthetic price data (50 data points, upward trend with noise)
    const int n = 50;
    close_.resize(n);
    high_.resize(n);
    low_.resize(n);
    volume_.resize(n);

    for (int i = 0; i < n; ++i) {
      double base = 100.0 + i * 0.5 + std::sin(i * 0.3) * 2.0;
      close_(i) = base;
      high_(i) = base + 1.0 + (i % 3) * 0.5;
      low_(i) = base - 1.0 - (i % 5) * 0.3;
      volume_(i) = 1000.0 + (i % 7) * 100.0;
    }
  }
};

// --- SMA ---
TEST_F(TechnicalIndicatorsTest, SMALength) {
  auto result = TechnicalIndicators::sma(close_, 7);
  EXPECT_EQ(result.size(), close_.size());
}

TEST_F(TechnicalIndicatorsTest, SMAFirstElementsAreNaN) {
  int period = 7;
  auto result = TechnicalIndicators::sma(close_, period);
  for (int i = 0; i < period - 1; ++i) {
    EXPECT_TRUE(std::isnan(result(i))) << "Expected NaN at index " << i;
  }
  EXPECT_FALSE(std::isnan(result(period - 1)));
}

TEST_F(TechnicalIndicatorsTest, SMACorrectValue) {
  int period = 3;
  auto result = TechnicalIndicators::sma(close_, period);
  double expected = (close_(0) + close_(1) + close_(2)) / 3.0;
  EXPECT_NEAR(result(2), expected, 1e-10);
}

// --- EMA ---
TEST_F(TechnicalIndicatorsTest, EMALength) {
  auto result = TechnicalIndicators::ema(close_, 12);
  EXPECT_EQ(result.size(), close_.size());
}

TEST_F(TechnicalIndicatorsTest, EMAFirstElementMatchesSMA) {
  int period = 5;
  auto ema_result = TechnicalIndicators::ema(close_, period);
  auto sma_result = TechnicalIndicators::sma(close_, period);
  // EMA seed is the first valid SMA value
  EXPECT_NEAR(ema_result(period - 1), sma_result(period - 1), 1e-10);
}

// --- RSI ---
TEST_F(TechnicalIndicatorsTest, RSIBoundedZeroToHundred) {
  auto result = TechnicalIndicators::rsi(close_, 14);
  for (int i = 14; i < result.size(); ++i) {
    if (!std::isnan(result(i))) {
      EXPECT_GE(result(i), 0.0);
      EXPECT_LE(result(i), 100.0);
    }
  }
}

// --- MACD ---
TEST_F(TechnicalIndicatorsTest, MACDReturnsThreeVectors) {
  auto result = TechnicalIndicators::macd(close_);
  EXPECT_EQ(result.macd.size(), close_.size());
  EXPECT_EQ(result.signal.size(), close_.size());
  EXPECT_EQ(result.histogram.size(), close_.size());
}

TEST_F(TechnicalIndicatorsTest, MACDHistogramIsMAcDMinusSignal) {
  auto result = TechnicalIndicators::macd(close_);
  for (int i = 30; i < close_.size(); ++i) {
    if (!std::isnan(result.macd(i)) && !std::isnan(result.signal(i))) {
      EXPECT_NEAR(result.histogram(i), result.macd(i) - result.signal(i),
                  1e-10);
    }
  }
}

// --- Bollinger Bands ---
TEST_F(TechnicalIndicatorsTest, BollingerBandsOrdering) {
  auto result = TechnicalIndicators::bollinger_bands(close_, 20, 2.0);
  for (int i = 20; i < close_.size(); ++i) {
    if (!std::isnan(result.upper(i)) && !std::isnan(result.lower(i))) {
      EXPECT_GE(result.upper(i), result.middle(i));
      EXPECT_LE(result.lower(i), result.middle(i));
    }
  }
}

// --- True Range / ATR ---
TEST_F(TechnicalIndicatorsTest, TrueRangePositive) {
  auto result = TechnicalIndicators::true_range(high_, low_, close_);
  for (int i = 1; i < result.size(); ++i) {
    if (!std::isnan(result(i))) {
      EXPECT_GE(result(i), 0.0);
    }
  }
}

TEST_F(TechnicalIndicatorsTest, ATRLength) {
  auto result = TechnicalIndicators::atr(high_, low_, close_, 14);
  EXPECT_EQ(result.size(), close_.size());
}

// --- OBV ---
TEST_F(TechnicalIndicatorsTest, OBVLength) {
  auto result = TechnicalIndicators::obv(close_, volume_);
  EXPECT_EQ(result.size(), close_.size());
}

// --- VWAP ---
TEST_F(TechnicalIndicatorsTest, VWAPLength) {
  auto result = TechnicalIndicators::vwap(high_, low_, close_, volume_);
  EXPECT_EQ(result.size(), close_.size());
}

// --- build_all ---
TEST_F(TechnicalIndicatorsTest, BuildAllOutputShape) {
  Eigen::MatrixXd ohlcv(close_.size(), 5);
  ohlcv.col(0) = close_ + Eigen::VectorXd::Ones(close_.size()); // open
  ohlcv.col(1) = high_;
  ohlcv.col(2) = low_;
  ohlcv.col(3) = close_;
  ohlcv.col(4) = volume_;

  IndicatorConfig config;
  std::vector<std::string> col_names;
  auto result = TechnicalIndicators::build_all(ohlcv, config, col_names);

  // Output should have more columns than input (5 + indicators)
  EXPECT_EQ(result.rows(), ohlcv.rows());
  EXPECT_GT(result.cols(), ohlcv.cols());
  EXPECT_FALSE(col_names.empty());
  EXPECT_EQ(static_cast<int>(col_names.size()), result.cols());
}
