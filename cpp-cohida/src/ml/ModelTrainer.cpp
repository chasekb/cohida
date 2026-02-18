#include "ml/ModelTrainer.h"
#include "utils/Logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

// Conditional XGBoost/LightGBM includes
#ifdef COHIDA_HAS_XGBOOST
#include <xgboost/c_api.h>
#endif

#ifdef COHIDA_HAS_LIGHTGBM
#include <lightgbm/c_api.h>
#endif

namespace ml {

// ----------------------------------------------------------- Config defaults
std::map<std::string, std::string>
TrainingConfig::get_default_hyperparameters() const {
  if (model_type == "xgboost") {
    if (task_type == "classification") {
      return {
          {"objective", "binary:logistic"},
          {"max_depth", "6"},
          {"learning_rate", "0.1"},
          {"n_estimators", "100"},
          {"subsample", "0.8"},
          {"colsample_bytree", "0.8"},
          {"seed", "42"},
      };
    }
    return {
        {"objective", "reg:squarederror"},
        {"max_depth", "6"},
        {"learning_rate", "0.1"},
        {"n_estimators", "100"},
        {"subsample", "0.8"},
        {"colsample_bytree", "0.8"},
        {"seed", "42"},
    };
  }
  if (model_type == "lightgbm") {
    if (task_type == "classification") {
      return {
          {"objective", "binary"},  {"max_depth", "6"},
          {"learning_rate", "0.1"}, {"n_estimators", "100"},
          {"subsample", "0.8"},     {"colsample_bytree", "0.8"},
          {"seed", "42"},
      };
    }
    return {
        {"objective", "regression"},
        {"max_depth", "6"},
        {"learning_rate", "0.1"},
        {"n_estimators", "100"},
        {"subsample", "0.8"},
        {"colsample_bytree", "0.8"},
        {"seed", "42"},
    };
  }
  return {};
}

// ----------------------------------------------------------- Constructor
ModelTrainer::ModelTrainer(const TrainingConfig &config,
                           const PreprocessorConfig &preprocessor_config)
    : config_(config), preprocessor_config_(preprocessor_config) {
  if (config_.hyperparameters.empty())
    config_.hyperparameters = config_.get_default_hyperparameters();
}

// ----------------------------------------------------------- Evaluate
std::map<std::string, double>
ModelTrainer::evaluate(const Eigen::VectorXd &y_true,
                       const Eigen::VectorXd &y_pred) const {

  std::map<std::string, double> metrics;
  const int n = static_cast<int>(y_true.size());
  if (n == 0)
    return metrics;

  if (config_.task_type == "classification") {
    // Accuracy
    int correct = 0;
    int tp = 0, fp = 0, fn = 0;
    for (int i = 0; i < n; ++i) {
      int pred = (y_pred(i) >= 0.5) ? 1 : 0;
      int actual = static_cast<int>(y_true(i));
      if (pred == actual)
        ++correct;
      if (pred == 1 && actual == 1)
        ++tp;
      if (pred == 1 && actual == 0)
        ++fp;
      if (pred == 0 && actual == 1)
        ++fn;
    }
    metrics["accuracy"] = static_cast<double>(correct) / n;
    double precision =
        (tp + fp > 0) ? static_cast<double>(tp) / (tp + fp) : 0.0;
    double recall = (tp + fn > 0) ? static_cast<double>(tp) / (tp + fn) : 0.0;
    metrics["precision"] = precision;
    metrics["recall"] = recall;
    metrics["f1_score"] = (precision + recall > 0)
                              ? 2.0 * precision * recall / (precision + recall)
                              : 0.0;
  } else {
    // Regression
    double mse = 0.0, mae = 0.0;
    double y_mean = y_true.mean();
    double ss_tot = 0.0, ss_res = 0.0;
    for (int i = 0; i < n; ++i) {
      double diff = y_true(i) - y_pred(i);
      mse += diff * diff;
      mae += std::abs(diff);
      ss_res += diff * diff;
      ss_tot += (y_true(i) - y_mean) * (y_true(i) - y_mean);
    }
    metrics["mse"] = mse / n;
    metrics["rmse"] = std::sqrt(mse / n);
    metrics["mae"] = mae / n;
    metrics["r2_score"] = (ss_tot > 0) ? (1.0 - ss_res / ss_tot) : 0.0;
  }
  return metrics;
}

// ----------------------------------------------------------- XGBoost Training
std::vector<uint8_t> ModelTrainer::train_xgboost(
    const Eigen::MatrixXd &X_train, const Eigen::VectorXd &y_train,
    const Eigen::MatrixXd &X_val, const Eigen::VectorXd &y_val) const {
#ifdef COHIDA_HAS_XGBOOST
  // Create DMatrix for training data
  DMatrixHandle dtrain;
  XGDMatrixCreateFromMat(X_train.data(), static_cast<bst_ulong>(X_train.rows()),
                         static_cast<bst_ulong>(X_train.cols()),
                         std::numeric_limits<float>::quiet_NaN(), &dtrain);

  // Set target labels
  std::vector<float> labels_train(y_train.size());
  for (int i = 0; i < static_cast<int>(y_train.size()); ++i)
    labels_train[static_cast<size_t>(i)] = static_cast<float>(y_train(i));
  XGDMatrixSetFloatInfo(dtrain, "label", labels_train.data(),
                        static_cast<bst_ulong>(labels_train.size()));

  // Create DMatrix for validation data
  DMatrixHandle dval;
  XGDMatrixCreateFromMat(X_val.data(), static_cast<bst_ulong>(X_val.rows()),
                         static_cast<bst_ulong>(X_val.cols()),
                         std::numeric_limits<float>::quiet_NaN(), &dval);

  std::vector<float> labels_val(y_val.size());
  for (int i = 0; i < static_cast<int>(y_val.size()); ++i)
    labels_val[static_cast<size_t>(i)] = static_cast<float>(y_val(i));
  XGDMatrixSetFloatInfo(dval, "label", labels_val.data(),
                        static_cast<bst_ulong>(labels_val.size()));

  // Create Booster
  BoosterHandle booster;
  DMatrixHandle eval_dmats[] = {dtrain, dval};
  XGBoosterCreate(eval_dmats, 2, &booster);

  // Set hyperparameters
  for (const auto &[key, value] : config_.hyperparameters) {
    if (key != "n_estimators")
      XGBoosterSetParam(booster, key.c_str(), value.c_str());
  }

  // Training loop
  int n_rounds = 100;
  auto it = config_.hyperparameters.find("n_estimators");
  if (it != config_.hyperparameters.end())
    n_rounds = std::stoi(it->second);

  const char *eval_names[] = {"train", "val"};
  const char *eval_result = nullptr;
  for (int i = 0; i < n_rounds; ++i) {
    XGBoosterUpdateOneIter(booster, i, dtrain);
    XGBoosterEvalOneIter(booster, i, eval_dmats, eval_names, 2, &eval_result);
  }

  // Serialize model to buffer
  bst_ulong buf_len = 0;
  const char *buf_ptr = nullptr;
  XGBoosterSaveModelToBuffer(booster, "{\"format\": \"ubj\"}", &buf_len,
                             &buf_ptr);

  std::vector<uint8_t> model_data(buf_ptr, buf_ptr + buf_len);

  // Cleanup
  XGBoosterFree(booster);
  XGDMatrixFree(dtrain);
  XGDMatrixFree(dval);

  return model_data;
#else
  (void)X_train;
  (void)y_train;
  (void)X_val;
  (void)y_val;
  LOG_WARN("XGBoost not available - compiled without COHIDA_HAS_XGBOOST");
  return {};
#endif
}

// ----------------------------------------------------------- LightGBM Training
std::vector<uint8_t> ModelTrainer::train_lightgbm(
    const Eigen::MatrixXd &X_train, const Eigen::VectorXd &y_train,
    const Eigen::MatrixXd &X_val, const Eigen::VectorXd &y_val) const {
#ifdef COHIDA_HAS_LIGHTGBM
  // Build parameter string
  std::ostringstream params;
  for (const auto &[key, value] : config_.hyperparameters) {
    if (key != "n_estimators")
      params << key << "=" << value << " ";
  }
  params << "verbose=-1 ";

  // Create training dataset
  DatasetHandle train_data;
  std::vector<double> train_flat(
      X_train.data(), X_train.data() + X_train.rows() * X_train.cols());
  std::vector<float> train_labels(y_train.size());
  for (int i = 0; i < static_cast<int>(y_train.size()); ++i)
    train_labels[static_cast<size_t>(i)] = static_cast<float>(y_train(i));

  LGBM_DatasetCreateFromMat(train_flat.data(), C_API_DTYPE_FLOAT64,
                            static_cast<int32_t>(X_train.rows()),
                            static_cast<int32_t>(X_train.cols()), 1,
                            params.str().c_str(), nullptr, &train_data);
  LGBM_DatasetSetField(train_data, "label", train_labels.data(),
                       static_cast<int32_t>(train_labels.size()),
                       C_API_DTYPE_FLOAT32);

  // Create validation dataset
  DatasetHandle val_data;
  std::vector<double> val_flat(X_val.data(),
                               X_val.data() + X_val.rows() * X_val.cols());
  std::vector<float> val_labels(y_val.size());
  for (int i = 0; i < static_cast<int>(y_val.size()); ++i)
    val_labels[static_cast<size_t>(i)] = static_cast<float>(y_val(i));

  LGBM_DatasetCreateFromMat(val_flat.data(), C_API_DTYPE_FLOAT64,
                            static_cast<int32_t>(X_val.rows()),
                            static_cast<int32_t>(X_val.cols()), 1,
                            params.str().c_str(), train_data, &val_data);
  LGBM_DatasetSetField(val_data, "label", val_labels.data(),
                       static_cast<int32_t>(val_labels.size()),
                       C_API_DTYPE_FLOAT32);

  // Create booster
  BoosterHandle booster;
  LGBM_BoosterCreate(train_data, params.str().c_str(), &booster);
  LGBM_BoosterAddValidData(booster, val_data);

  // Training loop
  int n_rounds = 100;
  auto it = config_.hyperparameters.find("n_estimators");
  if (it != config_.hyperparameters.end())
    n_rounds = std::stoi(it->second);

  int is_finished = 0;
  for (int i = 0; i < n_rounds && !is_finished; ++i) {
    LGBM_BoosterUpdateOneIter(booster, &is_finished);
  }

  // Serialize model
  int64_t buf_len = 0;
  LGBM_BoosterSaveModelToString(booster, 0, -1, 0, &buf_len, nullptr);
  std::vector<char> buf(static_cast<size_t>(buf_len));
  LGBM_BoosterSaveModelToString(booster, 0, -1, buf_len, &buf_len, buf.data());

  std::vector<uint8_t> model_data(buf.begin(), buf.end());

  // Cleanup
  LGBM_BoosterFree(booster);
  LGBM_DatasetFree(val_data);
  LGBM_DatasetFree(train_data);

  return model_data;
#else
  (void)X_train;
  (void)y_train;
  (void)X_val;
  (void)y_val;
  LOG_WARN("LightGBM not available - compiled without COHIDA_HAS_LIGHTGBM");
  return {};
#endif
}

// ----------------------------------------------------------- XGBoost Predict
Eigen::VectorXd
ModelTrainer::predict_xgboost(const std::vector<uint8_t> &model_data,
                              const Eigen::MatrixXd &X) const {
#ifdef COHIDA_HAS_XGBOOST
  // Load model from buffer
  BoosterHandle booster;
  XGBoosterCreate(nullptr, 0, &booster);
  XGBoosterLoadModelFromBuffer(booster, model_data.data(), model_data.size());

  // Create DMatrix
  DMatrixHandle dmat;
  XGDMatrixCreateFromMat(X.data(), static_cast<bst_ulong>(X.rows()),
                         static_cast<bst_ulong>(X.cols()),
                         std::numeric_limits<float>::quiet_NaN(), &dmat);

  // Predict
  bst_ulong out_len = 0;
  const float *out_result = nullptr;
  XGBoosterPredict(booster, dmat, 0, 0, 0, &out_len, &out_result);

  Eigen::VectorXd predictions(static_cast<int>(out_len));
  for (bst_ulong i = 0; i < out_len; ++i)
    predictions(static_cast<int>(i)) = static_cast<double>(out_result[i]);

  XGDMatrixFree(dmat);
  XGBoosterFree(booster);
  return predictions;
#else
  (void)model_data;
  (void)X;
  return Eigen::VectorXd::Zero(X.rows());
#endif
}

// ----------------------------------------------------------- LightGBM Predict
Eigen::VectorXd
ModelTrainer::predict_lightgbm(const std::vector<uint8_t> &model_data,
                               const Eigen::MatrixXd &X) const {
#ifdef COHIDA_HAS_LIGHTGBM
  // Load model from string
  BoosterHandle booster;
  std::string model_str(model_data.begin(), model_data.end());
  int n_iters = 0;
  LGBM_BoosterLoadModelFromString(model_str.c_str(), &n_iters, &booster);

  // Predict
  std::vector<double> flat(X.data(), X.data() + X.rows() * X.cols());
  int64_t out_len = 0;
  std::vector<double> out_result(static_cast<size_t>(X.rows()));

  LGBM_BoosterPredictForMat(
      booster, flat.data(), C_API_DTYPE_FLOAT64, static_cast<int32_t>(X.rows()),
      static_cast<int32_t>(X.cols()), 1, C_API_PREDICT_NORMAL, 0, -1, "",
      &out_len, out_result.data());

  Eigen::VectorXd predictions(static_cast<int>(out_len));
  for (int i = 0; i < static_cast<int>(out_len); ++i)
    predictions(i) = out_result[static_cast<size_t>(i)];

  LGBM_BoosterFree(booster);
  return predictions;
#else
  (void)model_data;
  (void)X;
  return Eigen::VectorXd::Zero(X.rows());
#endif
}

// ----------------------------------------------------------- Feature
// importance
std::map<std::string, double> ModelTrainer::get_feature_importance(
    const std::vector<uint8_t> &model_data,
    const std::vector<std::string> &feature_names) const {
  std::map<std::string, double> importance;
  (void)model_data;
  // Basic stub - would extract from model internals
  for (const auto &name : feature_names) {
    importance[name] = 0.0;
  }
  return importance;
}

// ----------------------------------------------------------- Train
TrainingResult ModelTrainer::train(
    const Eigen::MatrixXd &X, const Eigen::VectorXd &y,
    const std::vector<std::string> &feature_names,
    std::function<void(const std::string &)> progress_callback) {

  auto start = std::chrono::steady_clock::now();
  TrainingResult result;
  result.feature_names = feature_names;

  try {
    if (progress_callback)
      progress_callback("Splitting data...");

    // Split data
    DataSplitter splitter;
    auto split = splitter.split(X, y);

    LOG_INFO("Training data: {} samples, {} features", split.train_size(),
             static_cast<int>(X.cols()));

    if (progress_callback)
      progress_callback("Preprocessing features...");

    // Preprocess
    Preprocessor preprocessor(preprocessor_config_);
    auto X_train = preprocessor.fit_transform(split.train);
    auto X_val = preprocessor.transform(split.val);
    auto X_test = preprocessor.transform(split.test);
    result.preprocessor_config = preprocessor.get_config();

    if (progress_callback)
      progress_callback("Training " + config_.model_type + " model...");

    // Train model
    if (config_.model_type == "xgboost") {
      result.model_data =
          train_xgboost(X_train, split.train_target, X_val, split.val_target);
    } else if (config_.model_type == "lightgbm") {
      result.model_data =
          train_lightgbm(X_train, split.train_target, X_val, split.val_target);
    } else {
      throw std::invalid_argument("Unknown model type: " + config_.model_type);
    }

    if (result.model_data.empty()) {
      result.success = false;
      result.error_message =
          config_.model_type +
          " library not available (compile with COHIDA_HAS_" +
          (config_.model_type == "xgboost" ? "XGBOOST" : "LIGHTGBM") + ")";
      return result;
    }

    if (progress_callback)
      progress_callback("Evaluating model...");

    // Predict and evaluate on test set
    Eigen::VectorXd y_pred;
    if (config_.model_type == "xgboost")
      y_pred = predict_xgboost(result.model_data, X_test);
    else
      y_pred = predict_lightgbm(result.model_data, X_test);

    result.metrics = evaluate(split.test_target, y_pred);

    // Feature importance
    result.feature_importance =
        get_feature_importance(result.model_data, feature_names);

    auto end = std::chrono::steady_clock::now();
    result.training_time_seconds =
        std::chrono::duration<double>(end - start).count();
    result.success = true;

    LOG_INFO("Training complete in {:.2f}s", result.training_time_seconds);
    for (const auto &[k, v] : result.metrics)
      LOG_INFO("  {}: {:.4f}", k, v);

  } catch (const std::exception &e) {
    result.success = false;
    result.error_message = e.what();
    LOG_ERROR("Training failed: {}", e.what());
  }

  return result;
}

// ----------------------------------------------------------- Predict (public)
Eigen::VectorXd ModelTrainer::predict(const std::vector<uint8_t> &model_data,
                                      const Eigen::MatrixXd &X) const {
  if (config_.model_type == "xgboost")
    return predict_xgboost(model_data, X);
  if (config_.model_type == "lightgbm")
    return predict_lightgbm(model_data, X);
  throw std::invalid_argument("Unknown model type: " + config_.model_type);
}

} // namespace ml
