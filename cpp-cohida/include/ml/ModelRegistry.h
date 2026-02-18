#pragma once

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ml {

/**
 * @brief Metadata for a trained model.
 */
struct ModelMetadata {
  std::string model_id;
  std::string model_type; // "xgboost" or "lightgbm"
  std::string version;
  std::string symbol;
  std::string granularity;
  std::string created_at; // ISO-8601
  std::map<std::string, std::string> hyperparameters;
  std::map<std::string, double> metrics;
  std::vector<std::string> feature_names;
  std::map<std::string, std::string> preprocessor_config;

  nlohmann::json to_json() const;
  static ModelMetadata from_json(const nlohmann::json &j);
};

/**
 * @brief File-based model registry for versioning, storage, and management.
 *
 * Directory structure:
 *   registry_path/
 *     artifacts/   - serialized model files
 *     metadata/    - JSON metadata files
 *     preprocessors/ - serialized preprocessor state
 */
class ModelRegistry {
public:
  explicit ModelRegistry(const std::string &registry_path = "./models");

  /**
   * @brief Save model data and metadata to registry.
   * @return Model ID of saved model
   */
  std::string save_model(const std::vector<uint8_t> &model_data,
                         const ModelMetadata &metadata,
                         const std::vector<uint8_t> &preprocessor_data = {});

  /**
   * @brief Load model data from registry.
   * @return Tuple of (model bytes, metadata, preprocessor bytes)
   */
  struct LoadResult {
    std::vector<uint8_t> model_data;
    ModelMetadata metadata;
    std::vector<uint8_t> preprocessor_data;
  };
  LoadResult load_model(const std::string &model_id) const;

  /// List models with optional filters
  std::vector<ModelMetadata>
  list_models(const std::string &symbol = "",
              const std::string &model_type = "") const;

  /// Get metadata for a specific model
  ModelMetadata get_metadata(const std::string &model_id) const;

  /// Delete a model from registry
  void delete_model(const std::string &model_id);

  /// Compare models by a specific metric
  std::vector<std::pair<std::string, double>>
  compare_models(const std::vector<std::string> &model_ids,
                 const std::string &metric = "accuracy") const;

  /// Generate a unique model ID
  static std::string generate_model_id(const std::string &model_type,
                                       const std::string &symbol,
                                       const std::string &version = "");

  /// Check if model exists
  bool model_exists(const std::string &model_id) const;

  /// Get registry path
  const std::filesystem::path &path() const { return registry_path_; }

private:
  std::filesystem::path registry_path_;
  std::filesystem::path models_dir_;
  std::filesystem::path metadata_dir_;
  std::filesystem::path preprocessors_dir_;
};

} // namespace ml
