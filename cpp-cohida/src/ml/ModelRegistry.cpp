#include "ml/ModelRegistry.h"
#include "utils/Logger.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ml {

using json = nlohmann::json;

// ----------------------------------------------------------- ModelMetadata
json ModelMetadata::to_json() const {
  json j;
  j["model_id"] = model_id;
  j["model_type"] = model_type;
  j["version"] = version;
  j["symbol"] = symbol;
  j["granularity"] = granularity;
  j["created_at"] = created_at;
  j["hyperparameters"] = hyperparameters;
  j["metrics"] = metrics;
  j["feature_names"] = feature_names;
  j["preprocessor_config"] = preprocessor_config;
  return j;
}

ModelMetadata ModelMetadata::from_json(const json &j) {
  ModelMetadata m;
  m.model_id = j.value("model_id", "");
  m.model_type = j.value("model_type", "");
  m.version = j.value("version", "");
  m.symbol = j.value("symbol", "");
  m.granularity = j.value("granularity", "");
  m.created_at = j.value("created_at", "");

  if (j.contains("hyperparameters") && j["hyperparameters"].is_object()) {
    for (auto &[key, val] : j["hyperparameters"].items())
      m.hyperparameters[key] = val.get<std::string>();
  }
  if (j.contains("metrics") && j["metrics"].is_object()) {
    for (auto &[key, val] : j["metrics"].items())
      m.metrics[key] = val.get<double>();
  }
  if (j.contains("feature_names") && j["feature_names"].is_array()) {
    m.feature_names = j["feature_names"].get<std::vector<std::string>>();
  }
  if (j.contains("preprocessor_config") &&
      j["preprocessor_config"].is_object()) {
    for (auto &[key, val] : j["preprocessor_config"].items())
      m.preprocessor_config[key] = val.get<std::string>();
  }
  return m;
}

// ----------------------------------------------------------- ModelRegistry
ModelRegistry::ModelRegistry(const std::string &registry_path)
    : registry_path_(registry_path), models_dir_(registry_path_ / "artifacts"),
      metadata_dir_(registry_path_ / "metadata"),
      preprocessors_dir_(registry_path_ / "preprocessors") {
  std::filesystem::create_directories(models_dir_);
  std::filesystem::create_directories(metadata_dir_);
  std::filesystem::create_directories(preprocessors_dir_);
  LOG_INFO("Model registry initialized at {}", registry_path_.string());
}

std::string
ModelRegistry::save_model(const std::vector<uint8_t> &model_data,
                          const ModelMetadata &metadata,
                          const std::vector<uint8_t> &preprocessor_data) {

  const auto &model_id = metadata.model_id;
  if (model_exists(model_id))
    throw std::runtime_error("Model '" + model_id +
                             "' already exists in registry");

  // Save model artifact
  auto model_path = models_dir_ / (model_id + ".bin");
  std::ofstream model_file(model_path, std::ios::binary);
  if (!model_file)
    throw std::runtime_error("Cannot write model file: " + model_path.string());
  model_file.write(reinterpret_cast<const char *>(model_data.data()),
                   static_cast<std::streamsize>(model_data.size()));
  model_file.close();
  LOG_INFO("Model artifact saved: {}", model_id);

  // Save metadata
  auto metadata_path = metadata_dir_ / (model_id + ".json");
  std::ofstream meta_file(metadata_path);
  if (!meta_file)
    throw std::runtime_error("Cannot write metadata file");
  meta_file << metadata.to_json().dump(2);
  meta_file.close();

  // Save preprocessor if provided
  if (!preprocessor_data.empty()) {
    auto prep_path = preprocessors_dir_ / (model_id + ".bin");
    std::ofstream prep_file(prep_path, std::ios::binary);
    prep_file.write(reinterpret_cast<const char *>(preprocessor_data.data()),
                    static_cast<std::streamsize>(preprocessor_data.size()));
    prep_file.close();
  }

  LOG_INFO("Model saved to registry: {}", model_id);
  return model_id;
}

ModelRegistry::LoadResult
ModelRegistry::load_model(const std::string &model_id) const {
  if (!model_exists(model_id))
    throw std::runtime_error("Model '" + model_id + "' not found in registry");

  LoadResult result;

  // Load model artifact
  auto model_path = models_dir_ / (model_id + ".bin");
  std::ifstream model_file(model_path, std::ios::binary | std::ios::ate);
  auto size = model_file.tellg();
  model_file.seekg(0);
  result.model_data.resize(static_cast<size_t>(size));
  model_file.read(reinterpret_cast<char *>(result.model_data.data()), size);
  model_file.close();

  // Load metadata
  result.metadata = get_metadata(model_id);

  // Load preprocessor if it exists
  auto prep_path = preprocessors_dir_ / (model_id + ".bin");
  if (std::filesystem::exists(prep_path)) {
    std::ifstream prep_file(prep_path, std::ios::binary | std::ios::ate);
    auto prep_size = prep_file.tellg();
    prep_file.seekg(0);
    result.preprocessor_data.resize(static_cast<size_t>(prep_size));
    prep_file.read(reinterpret_cast<char *>(result.preprocessor_data.data()),
                   prep_size);
  }

  LOG_INFO("Model loaded from registry: {}", model_id);
  return result;
}

std::vector<ModelMetadata>
ModelRegistry::list_models(const std::string &symbol,
                           const std::string &model_type) const {
  std::vector<ModelMetadata> models;

  if (!std::filesystem::exists(metadata_dir_))
    return models;

  for (const auto &entry : std::filesystem::directory_iterator(metadata_dir_)) {
    if (entry.path().extension() != ".json")
      continue;

    std::ifstream f(entry.path());
    json j;
    f >> j;
    auto metadata = ModelMetadata::from_json(j);

    // Apply filters
    if (!symbol.empty() && metadata.symbol != symbol)
      continue;
    if (!model_type.empty() && metadata.model_type != model_type)
      continue;

    models.push_back(std::move(metadata));
  }

  // Sort by creation date (newest first)
  std::sort(models.begin(), models.end(),
            [](const ModelMetadata &a, const ModelMetadata &b) {
              return a.created_at > b.created_at;
            });

  return models;
}

ModelMetadata ModelRegistry::get_metadata(const std::string &model_id) const {
  auto metadata_path = metadata_dir_ / (model_id + ".json");
  if (!std::filesystem::exists(metadata_path))
    throw std::runtime_error("Model '" + model_id + "' not found in registry");

  std::ifstream f(metadata_path);
  json j;
  f >> j;
  return ModelMetadata::from_json(j);
}

void ModelRegistry::delete_model(const std::string &model_id) {
  if (!model_exists(model_id))
    throw std::runtime_error("Model '" + model_id + "' not found in registry");

  auto model_path = models_dir_ / (model_id + ".bin");
  auto metadata_path = metadata_dir_ / (model_id + ".json");
  auto prep_path = preprocessors_dir_ / (model_id + ".bin");

  std::filesystem::remove(model_path);
  std::filesystem::remove(metadata_path);
  std::filesystem::remove(prep_path);

  LOG_INFO("Model deleted from registry: {}", model_id);
}

std::vector<std::pair<std::string, double>>
ModelRegistry::compare_models(const std::vector<std::string> &model_ids,
                              const std::string &metric) const {
  std::vector<std::pair<std::string, double>> comparisons;

  for (const auto &id : model_ids) {
    try {
      auto metadata = get_metadata(id);
      auto it = metadata.metrics.find(metric);
      if (it != metadata.metrics.end())
        comparisons.emplace_back(id, it->second);
      else
        LOG_WARN("Metric '{}' not found for model '{}'", metric, id);
    } catch (...) {
      LOG_WARN("Model '{}' not found in registry", id);
    }
  }

  // Sort descending by metric value
  std::sort(comparisons.begin(), comparisons.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  return comparisons;
}

std::string ModelRegistry::generate_model_id(const std::string &model_type,
                                             const std::string &symbol,
                                             const std::string &version) {
  std::string ver = version;
  if (ver.empty()) {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val = *std::gmtime(&time_t_val);
    std::ostringstream oss;
    oss << "v_" << std::put_time(&tm_val, "%Y%m%d_%H%M%S");
    ver = oss.str();
  }

  // Sanitize symbol: replace '-' with '_'
  std::string safe_symbol = symbol;
  std::replace(safe_symbol.begin(), safe_symbol.end(), '-', '_');

  return model_type + "_" + safe_symbol + "_" + ver;
}

bool ModelRegistry::model_exists(const std::string &model_id) const {
  return std::filesystem::exists(models_dir_ / (model_id + ".bin")) &&
         std::filesystem::exists(metadata_dir_ / (model_id + ".json"));
}

} // namespace ml
