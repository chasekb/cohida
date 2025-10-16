"""
Model registry for versioning, storage, and management of trained ML models.

Handles model persistence, metadata tracking, and comparison utilities.
"""

from __future__ import annotations

import os
import json
import joblib
from pathlib import Path
from typing import Optional, Dict, Any, List
from datetime import datetime
from dataclasses import dataclass, asdict
import structlog

logger = structlog.get_logger(__name__)


@dataclass
class ModelMetadata:
    """Metadata for a trained model."""
    
    model_id: str
    model_type: str
    version: str
    symbol: str
    granularity: str
    created_at: str
    hyperparameters: Dict[str, Any]
    metrics: Dict[str, float]
    feature_names: List[str]
    preprocessor_config: Dict[str, Any]
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> ModelMetadata:
        """Create from dictionary."""
        return cls(**data)


class ModelRegistry:
    """
    Manages ML model storage, versioning, and metadata.
    
    Models are saved to disk with associated metadata for
    reproducibility and comparison.
    """
    
    def __init__(self, registry_path: Optional[str] = None):
        """
        Initialize model registry.
        
        Args:
            registry_path: Path to registry directory (default: './models')
        """
        self.registry_path = Path(registry_path or './models')
        self.registry_path.mkdir(parents=True, exist_ok=True)
        
        # Create subdirectories
        self.models_dir = self.registry_path / 'artifacts'
        self.metadata_dir = self.registry_path / 'metadata'
        self.preprocessors_dir = self.registry_path / 'preprocessors'
        
        self.models_dir.mkdir(exist_ok=True)
        self.metadata_dir.mkdir(exist_ok=True)
        self.preprocessors_dir.mkdir(exist_ok=True)
        
        logger.info("Model registry initialized", path=str(self.registry_path))
    
    def save_model(
        self,
        model: Any,
        metadata: ModelMetadata,
        preprocessor: Optional[Any] = None,
    ) -> str:
        """
        Save model, metadata, and optional preprocessor to registry.
        
        Args:
            model: Trained model object (must be picklable)
            metadata: Model metadata
            preprocessor: Optional preprocessor object
        
        Returns:
            Model ID of saved model
        
        Raises:
            ValueError: If model_id already exists
        """
        model_id = metadata.model_id
        
        # Check if model already exists
        if self._model_exists(model_id):
            raise ValueError(f"Model '{model_id}' already exists in registry")
        
        # Save model artifact
        model_path = self.models_dir / f"{model_id}.joblib"
        joblib.dump(model, model_path)
        logger.info("Model artifact saved", model_id=model_id, path=str(model_path))
        
        # Save metadata
        metadata_path = self.metadata_dir / f"{model_id}.json"
        with open(metadata_path, 'w') as f:
            json.dump(metadata.to_dict(), f, indent=2)
        logger.info("Model metadata saved", model_id=model_id)
        
        # Save preprocessor if provided
        if preprocessor:
            preprocessor_path = self.preprocessors_dir / f"{model_id}.joblib"
            joblib.dump(preprocessor, preprocessor_path)
            logger.info("Preprocessor saved", model_id=model_id)
        
        return model_id
    
    def load_model(
        self,
        model_id: str,
        load_preprocessor: bool = True,
    ) -> tuple[Any, ModelMetadata, Optional[Any]]:
        """
        Load model, metadata, and optional preprocessor from registry.
        
        Args:
            model_id: ID of model to load
            load_preprocessor: If True, also load preprocessor if it exists
        
        Returns:
            Tuple of (model, metadata, preprocessor)
        
        Raises:
            FileNotFoundError: If model doesn't exist
        """
        if not self._model_exists(model_id):
            raise FileNotFoundError(f"Model '{model_id}' not found in registry")
        
        # Load model artifact
        model_path = self.models_dir / f"{model_id}.joblib"
        model = joblib.load(model_path)
        logger.info("Model artifact loaded", model_id=model_id)
        
        # Load metadata
        metadata_path = self.metadata_dir / f"{model_id}.json"
        with open(metadata_path, 'r') as f:
            metadata_dict = json.load(f)
        metadata = ModelMetadata.from_dict(metadata_dict)
        logger.info("Model metadata loaded", model_id=model_id)
        
        # Load preprocessor if requested
        preprocessor = None
        if load_preprocessor:
            preprocessor_path = self.preprocessors_dir / f"{model_id}.joblib"
            if preprocessor_path.exists():
                preprocessor = joblib.load(preprocessor_path)
                logger.info("Preprocessor loaded", model_id=model_id)
        
        return model, metadata, preprocessor
    
    def list_models(
        self,
        symbol: Optional[str] = None,
        model_type: Optional[str] = None,
    ) -> List[ModelMetadata]:
        """
        List all models in registry with optional filtering.
        
        Args:
            symbol: Filter by symbol (optional)
            model_type: Filter by model type (optional)
        
        Returns:
            List of model metadata matching filters
        """
        models = []
        
        for metadata_file in self.metadata_dir.glob('*.json'):
            with open(metadata_file, 'r') as f:
                metadata_dict = json.load(f)
            
            metadata = ModelMetadata.from_dict(metadata_dict)
            
            # Apply filters
            if symbol and metadata.symbol != symbol:
                continue
            
            if model_type and metadata.model_type != model_type:
                continue
            
            models.append(metadata)
        
        # Sort by creation date (newest first)
        models.sort(key=lambda m: m.created_at, reverse=True)
        
        return models
    
    def get_metadata(self, model_id: str) -> ModelMetadata:
        """
        Get metadata for a specific model.
        
        Args:
            model_id: ID of model
        
        Returns:
            Model metadata
        
        Raises:
            FileNotFoundError: If model doesn't exist
        """
        if not self._model_exists(model_id):
            raise FileNotFoundError(f"Model '{model_id}' not found in registry")
        
        metadata_path = self.metadata_dir / f"{model_id}.json"
        with open(metadata_path, 'r') as f:
            metadata_dict = json.load(f)
        
        return ModelMetadata.from_dict(metadata_dict)
    
    def delete_model(self, model_id: str) -> None:
        """
        Delete a model and its metadata from registry.
        
        Args:
            model_id: ID of model to delete
        
        Raises:
            FileNotFoundError: If model doesn't exist
        """
        if not self._model_exists(model_id):
            raise FileNotFoundError(f"Model '{model_id}' not found in registry")
        
        # Delete model artifact
        model_path = self.models_dir / f"{model_id}.joblib"
        if model_path.exists():
            model_path.unlink()
        
        # Delete metadata
        metadata_path = self.metadata_dir / f"{model_id}.json"
        if metadata_path.exists():
            metadata_path.unlink()
        
        # Delete preprocessor
        preprocessor_path = self.preprocessors_dir / f"{model_id}.joblib"
        if preprocessor_path.exists():
            preprocessor_path.unlink()
        
        logger.info("Model deleted from registry", model_id=model_id)
    
    def compare_models(
        self,
        model_ids: List[str],
        metric: str = 'accuracy',
    ) -> List[tuple[str, float]]:
        """
        Compare models by a specific metric.
        
        Args:
            model_ids: List of model IDs to compare
            metric: Metric name to compare (default: 'accuracy')
        
        Returns:
            List of (model_id, metric_value) tuples sorted by metric value
        """
        comparisons = []
        
        for model_id in model_ids:
            try:
                metadata = self.get_metadata(model_id)
                if metric in metadata.metrics:
                    comparisons.append((model_id, metadata.metrics[metric]))
                else:
                    logger.warning(
                        f"Metric '{metric}' not found for model '{model_id}'"
                    )
            except FileNotFoundError:
                logger.warning(f"Model '{model_id}' not found in registry")
        
        # Sort by metric value (descending)
        comparisons.sort(key=lambda x: x[1], reverse=True)
        
        return comparisons
    
    def generate_model_id(
        self,
        model_type: str,
        symbol: str,
        version: Optional[str] = None,
    ) -> str:
        """
        Generate a unique model ID.
        
        Args:
            model_type: Type of model (e.g., 'xgboost', 'lightgbm')
            symbol: Trading symbol
            version: Semantic version (auto-generated if None)
        
        Returns:
            Unique model ID
        """
        if version is None:
            # Auto-generate version based on timestamp
            timestamp = datetime.utcnow().strftime('%Y%m%d_%H%M%S')
            version = f"v_{timestamp}"
        
        model_id = f"{model_type}_{symbol.replace('-', '_')}_{version}"
        
        return model_id
    
    def _model_exists(self, model_id: str) -> bool:
        """Check if model exists in registry."""
        model_path = self.models_dir / f"{model_id}.joblib"
        metadata_path = self.metadata_dir / f"{model_id}.json"
        return model_path.exists() and metadata_path.exists()



