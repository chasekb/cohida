"""
Unit tests for configuration management.
Tests configuration loading, validation, and credential handling.
"""

import pytest
import os
import tempfile
from pathlib import Path
from unittest.mock import patch, mock_open

from src.config import Config


class TestConfig:
    """Test cases for Config class."""
    
    def test_config_initialization(self):
        """Test basic configuration initialization."""
        config = Config()
        
        # Test that configuration attributes exist
        assert hasattr(config, 'db_host')
        assert hasattr(config, 'db_port')
        assert hasattr(config, 'db_name')
        assert hasattr(config, 'api_key')
        assert hasattr(config, 'log_level')
    

    def test_api_config_defaults(self):
        """Test API configuration defaults."""
        config = Config()
        
        assert config.sandbox_mode is False
        assert config.log_level == "INFO"
        assert config.log_format == "json"
    
    def test_read_credential_file_success(self):
        """Test successful credential file reading."""
        config = Config()
        
        test_credential = "test_password"
        
        with patch("builtins.open", mock_open(read_data=test_credential)):
            result = config._read_credential_file("TEST_FILE", "test_path")
            assert result == test_credential
    
    def test_read_credential_file_not_found(self):
        """Test credential file not found handling."""
        config = Config()
        
        with patch("builtins.open", side_effect=FileNotFoundError):
            result = config._read_credential_file("TEST_FILE", "nonexistent_path")
            assert result is None
    
    def test_read_credential_file_error(self):
        """Test credential file reading error handling."""
        config = Config()
        
        with patch("builtins.open", side_effect=PermissionError):
            result = config._read_credential_file("TEST_FILE", "test_path")
            assert result is None
    
    def test_database_url_generation(self):
        """Test database URL generation."""
        config = Config()
        config.db_user = "test_user"
        config.db_password = "test_password"
        config.db_host = "test_host"
        config.db_port = 5432
        config.db_name = "test_db"
        
        expected_url = "postgresql://test_user:test_password@test_host:5432/test_db"
        assert config.database_url == expected_url
    
    def test_api_credentials_validation(self):
        """Test API credentials validation."""
        config = Config()
        
        # Test with missing credentials
        config.api_key = None
        config.api_secret = None
        config.api_passphrase = None
        assert config.api_credentials_valid is False
        
        # Test with partial credentials
        config.api_key = "test_key"
        config.api_secret = None
        config.api_passphrase = None
        assert config.api_credentials_valid is False
        
        # Test with all credentials
        config.api_key = "test_key"
        config.api_secret = "test_secret"
        config.api_passphrase = "test_passphrase"
        assert config.api_credentials_valid is True
    
    def test_database_credentials_validation(self):
        """Test database credentials validation."""
        config = Config()
        
        # Test with missing credentials
        config.db_user = None
        config.db_password = None
        assert config.database_credentials_valid is False
        
        # Test with partial credentials
        config.db_user = "test_user"
        config.db_password = None
        assert config.database_credentials_valid is False
        
        # Test with all credentials
        config.db_user = "test_user"
        config.db_password = "test_password"
        assert config.database_credentials_valid is True
    
    @patch.dict(os.environ, {
        'POSTGRES_HOST': 'custom_host',
        'POSTGRES_PORT': '3306',
        'POSTGRES_DB': 'custom_db',
        'LOG_LEVEL': 'DEBUG',
        'LOG_FORMAT': 'console'
    })
    def test_environment_variable_override(self):
        """Test that environment variables override defaults."""
        config = Config()
        
        assert config.db_host == "custom_host"
        assert config.db_port == 3306
        assert config.db_name == "custom_db"
        assert config.log_level == "DEBUG"
        assert config.log_format == "console"
    
    def test_sandbox_mode_parsing(self):
        """Test sandbox mode boolean parsing."""
        config = Config()
        
        # Test various true values
        for true_value in ['true', 'True', 'TRUE', '1', 'yes', 'Yes']:
            with patch.dict(os.environ, {'COINBASE_SANDBOX': true_value}):
                config._setup_api_config()
                assert config.sandbox_mode is True
        
        # Test various false values
        for false_value in ['false', 'False', 'FALSE', '0', 'no', 'No', '']:
            with patch.dict(os.environ, {'COINBASE_SANDBOX': false_value}):
                config._setup_api_config()
                assert config.sandbox_mode is False
