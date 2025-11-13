"""
Configuration management for the Coinbase Historical Data Retrieval application.
Handles environment variables, database settings, and application parameters.
"""

import os
from pathlib import Path
from typing import Optional
from dotenv import load_dotenv
import structlog

logger = structlog.get_logger(__name__)


class Config:
    """Centralized configuration management for the application."""
    
    def __init__(self):
        """Initialize configuration by loading environment variables."""
        self._load_environment()
        self._setup_database_config()
        self._setup_api_config()
        self._setup_logging_config()
    
    def _load_environment(self) -> None:
        """Load environment variables from .env file."""
        env_file = Path(".env")
        if env_file.exists():
            load_dotenv(env_file)
            logger.info("Environment variables loaded from .env file")
        else:
            logger.warning("No .env file found, using system environment variables")
    
    def _setup_database_config(self) -> None:
        """Configure database connection settings."""
        self.db_host = os.getenv("POSTGRES_HOST")
        self.db_port = int(os.getenv("POSTGRES_PORT")) if os.getenv("POSTGRES_PORT") else None
        self.base_db_name = os.getenv("POSTGRES_DB")

        # Read credentials from files
        self.db_user = self._read_credential_file("POSTGRES_USER_FILE")
        self.db_password = self._read_credential_file("POSTGRES_PASSWORD_FILE")

        # Database schema configuration
        self.db_schema = os.getenv("DB_SCHEMA")
        self.db_table = os.getenv("DB_TABLE")

        # Output configuration
        self.output_dir = os.getenv("OUTPUT_DIR")

        # Granularity-based table naming (database separation disabled)
        granularity_suffix_env = os.getenv("GRANULARITY_TABLE_SUFFIX")
        self.granularity_table_suffix = granularity_suffix_env.lower() == "true" if granularity_suffix_env else False
        
        logger.info("Database configuration initialized", 
                   host=self.db_host, port=self.db_port, base_database=self.base_db_name)
    
    def _setup_api_config(self) -> None:
        """Configure Coinbase API settings."""
        self.api_key = os.getenv("COINBASE_API_KEY")
        self.api_secret = os.getenv("COINBASE_API_SECRET")
        self.api_passphrase = os.getenv("COINBASE_API_PASSPHRASE")
        self.sandbox_mode = os.getenv("COINBASE_SANDBOX", "false").lower() == "true"
        
        if not all([self.api_key, self.api_secret, self.api_passphrase]):
            logger.warning("Coinbase API credentials not fully configured")
    
    def _setup_logging_config(self) -> None:
        """Configure logging settings."""
        self.log_level = os.getenv("LOG_LEVEL", "INFO")
        self.log_format = os.getenv("LOG_FORMAT", "json")
        
        # Configure structured logging
        structlog.configure(
            processors=[
                structlog.stdlib.filter_by_level,
                structlog.stdlib.add_logger_name,
                structlog.stdlib.add_log_level,
                structlog.processors.TimeStamper(fmt="iso"),
                structlog.processors.StackInfoRenderer(),
                structlog.processors.format_exc_info,
                structlog.processors.UnicodeDecoder(),
                structlog.processors.JSONRenderer() if self.log_format == "json" 
                else structlog.dev.ConsoleRenderer()
            ],
            context_class=dict,
            logger_factory=structlog.stdlib.LoggerFactory(),
            wrapper_class=structlog.stdlib.BoundLogger,
            cache_logger_on_first_use=True,
        )
    
    def _read_credential_file(self, env_var: str, default_path: Optional[str] = None) -> Optional[str]:
        """Read credential from file specified by environment variable or default path."""
        file_path = os.getenv(env_var, default_path)

        if not file_path:
            logger.error(f"Environment variable {env_var} not set and no default path provided")
            return None

        try:
            with open(file_path, 'r') as f:
                credential = f.read().strip()
                logger.debug(f"Credential loaded from {file_path}")
                return credential
        except FileNotFoundError:
            logger.error(f"Credential file not found: {file_path}")
            return None
        except Exception as e:
            logger.error(f"Error reading credential file {file_path}: {e}")
            return None
    
    @property
    def database_url(self) -> str:
        """Generate database connection URL."""
        return f"postgresql://{self.db_user}:{self.db_password}@{self.db_host}:{self.db_port or 5432}/{self.base_db_name}"
    
    @property
    def api_credentials_valid(self) -> bool:
        """Check if API credentials are properly configured."""
        return all([self.api_key, self.api_secret, self.api_passphrase])
    
    @property
    def database_credentials_valid(self) -> bool:
        """Check if database credentials are properly configured."""
        return all([self.db_host, self.db_port, self.base_db_name, self.db_user, self.db_password])
    
    def get_database_name(self, granularity: int = None) -> str:
        """Get database name (always returns base database name)."""
        return self.base_db_name
    
    def get_table_name(self, granularity: int = None) -> str:
        """Get table name, optionally with granularity suffix."""
        if granularity and self.granularity_table_suffix:
            granularity_suffix = self._get_granularity_suffix(granularity)
            return f"{self.db_table}_{granularity_suffix}"
        return self.db_table
    
    def _get_granularity_suffix(self, granularity: int) -> str:
        """Convert granularity in seconds to human-readable suffix."""
        granularity_map = {
            60: "1m",
            300: "5m", 
            900: "15m",
            3600: "1h",
            21600: "6h",
            86400: "1d"
        }
        return granularity_map.get(granularity, f"{granularity}s")


# Global configuration instance
config = Config()
