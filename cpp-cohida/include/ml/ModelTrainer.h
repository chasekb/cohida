#pragma once

#include "ml/DataSplitter.h"
#include "ml/Preprocessor.h"
#include <Eigen/Dense>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ml {

struct TrainingConfig {
  /// Model type: "xgboost" or "lightgbm"
  std::string model_type = "xgboost";

  /// Task type: "classification" or "regression"
  std::string task_type = "classification";

  /// Target column name (used for labeling, not indexing)
  std::string target_col = "target";

  /// Column indices to exclude from features
  std::vector<int> exclude_cols;

  /// Cross-validation folds
  int cv_folds = 5;

  /// Hyperparameters as string key-value pairs
  std::map<std::string, std::string> hyperparameters;

  /// Early stopping rounds
  int early_stopping_rounds = 50;

  /// Verbose training output
  bool verbose = false;

  /// Get default hyperparameters for the configured model/task
  std::map<std::string, std::string> get_default_hyperparameters() const;
};

struct TrainingResult {
  /// Serialized model bytes (for later deserialization)
  std::vector<uint8_t> model_data;

  /// Evaluation metrics
  std::map<std::string, double> metrics;

  /// Feature importance (feature_name -> importance)
  std::map<std::string, double> feature_importance;

  /// Feature names used during training
  std::vector<std::string> feature_names;

  /// Training time in seconds
  double training_time_seconds = 0.0;

  /// Best hyperparameters (if tuning was performed)
  std::optional<std::map<std::string, std::string>> best_params;

  /// Preprocessor config used
  std::map<std::string, std::string> preprocessor_config;

  /// Whether training was successful
  bool success = false;

  /// Error message if training failed
  std::string error_message;
};

/**
 * @brief Orchestrates ML model training with XGBoost/LightGBM.
 *
 * Handles the complete pipeline: preprocessing → splitting →
 * training → evaluation → result packaging.
 */
class ModelTrainer {
public:
  explicit ModelTrainer(
      const TrainingConfig &config = TrainingConfig{},
      const PreprocessorConfig &preprocessor_config = PreprocessorConfig{});

  /**
   * @brief Train model on feature matrix and target.
   *
   * @param X  Feature matrix
   * @param y  Target vector
   * @param feature_names  Column names for the features
   * @param progress_callback  Optional progress callback
   * @return TrainingResult with model data and metrics
   */
  TrainingResult
  train(const Eigen::MatrixXd &X, const Eigen::VectorXd &y,
        const std::vector<std::string> &feature_names,
        std::function<void(const std::string &)> progress_callback = nullptr);

  /**
   * @brief Predict using a trained model (serialized bytes).
   *
   * @param model_data  Serialized model bytes from TrainingResult
   * @param X  Feature matrix to predict on
   * @return Prediction vector
   */
  Eigen::VectorXd predict(const std::vector<uint8_t> &model_data,
                          const Eigen::MatrixXd &X) const;

  /// Get the training configuration
  const TrainingConfig &config() const { return config_; }

private:
  TrainingConfig config_;
  PreprocessorConfig preprocessor_config_;

  /// Evaluate model predictions vs ground truth
  std::map<std::string, double> evaluate(const Eigen::VectorXd &y_true,
                                         const Eigen::VectorXd &y_pred) const;

  /// Get feature importance from model bytes
  std::map<std::string, double>
  get_feature_importance(const std::vector<uint8_t> &model_data,
                         const std::vector<std::string> &feature_names) const;

  /// Train XGBoost model
  std::vector<uint8_t> train_xgboost(const Eigen::MatrixXd &X_train,
                                     const Eigen::VectorXd &y_train,
                                     const Eigen::MatrixXd &X_val,
                                     const Eigen::VectorXd &y_val) const;

  /// Train LightGBM model
  std::vector<uint8_t> train_lightgbm(const Eigen::MatrixXd &X_train,
                                      const Eigen::VectorXd &y_train,
                                      const Eigen::MatrixXd &X_val,
                                      const Eigen::VectorXd &y_val) const;

  /// Predict with XGBoost model
  Eigen::VectorXd predict_xgboost(const std::vector<uint8_t> &model_data,
                                  const Eigen::MatrixXd &X) const;

  /// Predict with LightGBM model
  Eigen::VectorXd predict_lightgbm(const std::vector<uint8_t> &model_data,
                                   const Eigen::MatrixXd &X) const;
};

} // namespace ml
