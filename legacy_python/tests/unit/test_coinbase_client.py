"""
Unit tests for Coinbase API client.
Tests authentication, connection management, and API interactions.
"""

import pytest
from unittest.mock import Mock, patch, MagicMock
from src.coinbase_client import CoinbaseClient


class TestCoinbaseClient:
    """Test cases for CoinbaseClient class."""
    
    @patch('src.coinbase_client.CoinbaseAdvancedTrader')
    @patch('src.coinbase_client.CoinbaseConfig')
    def test_successful_authentication(self, mock_config_class, mock_client_class):
        """Test successful authentication with valid credentials."""
        # Mock configuration
        mock_config = Mock()
        mock_config_class.return_value = mock_config
        
        # Mock client
        mock_client = Mock()
        mock_client_class.return_value = mock_client
        
        # Mock config to have valid credentials
        with patch('src.coinbase_client.config') as mock_config_obj:
            mock_config_obj.api_credentials_valid = True
            mock_config_obj.api_key = "test_key"
            mock_config_obj.api_secret = "test_secret"
            mock_config_obj.api_passphrase = "test_passphrase"
            mock_config_obj.sandbox_mode = False
            
            client = CoinbaseClient()
            
            # Verify configuration was called correctly
            mock_config_class.assert_called_once_with(
                api_key="test_key",
                api_secret="test_secret",
                passphrase="test_passphrase",
                sandbox=False
            )
            
            # Verify client was initialized
            mock_client_class.assert_called_once_with(mock_config)
            assert client.client == mock_client
    
    @patch('src.coinbase_client.config')
    def test_authentication_with_invalid_credentials(self, mock_config):
        """Test authentication failure with invalid credentials."""
        mock_config.api_credentials_valid = False
        
        with pytest.raises(ValueError, match="Missing required API credentials"):
            CoinbaseClient()
    
    @patch('src.coinbase_client.CoinbaseAdvancedTrader')
    @patch('src.coinbase_client.CoinbaseConfig')
    def test_authentication_exception(self, mock_config_class, mock_client_class):
        """Test authentication exception handling."""
        mock_client_class.side_effect = Exception("Authentication failed")
        
        with patch('src.coinbase_client.config') as mock_config_obj:
            mock_config_obj.api_credentials_valid = True
            mock_config_obj.api_key = "test_key"
            mock_config_obj.api_secret = "test_secret"
            mock_config_obj.api_passphrase = "test_passphrase"
            mock_config_obj.sandbox_mode = False
            
            with pytest.raises(Exception, match="Authentication failed"):
                CoinbaseClient()
    
    def test_test_connection_success(self):
        """Test successful connection test."""
        mock_client = Mock()
        mock_client.get_accounts.return_value = [{"id": "test_account"}]
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.test_connection()
        
        assert result is True
        mock_client.get_accounts.assert_called_once()
    
    def test_test_connection_failure(self):
        """Test connection test failure."""
        mock_client = Mock()
        mock_client.get_accounts.return_value = []
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.test_connection()
        
        assert result is False
    
    def test_test_connection_exception(self):
        """Test connection test with exception."""
        mock_client = Mock()
        mock_client.get_accounts.side_effect = Exception("Connection failed")
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.test_connection()
        
        assert result is False
    
    def test_test_connection_no_client(self):
        """Test connection test with no client."""
        client = CoinbaseClient()
        client.client = None
        
        result = client.test_connection()
        
        assert result is False
    
    def test_get_available_symbols_success(self):
        """Test successful symbol retrieval."""
        mock_client = Mock()
        mock_client.get_products.return_value = [
            {"product_id": "BTC-USD", "status": "online"},
            {"product_id": "ETH-USD", "status": "online"}
        ]
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_available_symbols()
        
        assert result is not None
        assert len(result) == 2
        mock_client.get_products.assert_called_once()
    
    def test_get_available_symbols_failure(self):
        """Test symbol retrieval failure."""
        mock_client = Mock()
        mock_client.get_products.return_value = None
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_available_symbols()
        
        assert result is None
    
    def test_get_available_symbols_exception(self):
        """Test symbol retrieval with exception."""
        mock_client = Mock()
        mock_client.get_products.side_effect = Exception("API error")
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_available_symbols()
        
        assert result is None
    
    def test_get_symbol_info_success(self):
        """Test successful symbol info retrieval."""
        mock_client = Mock()
        mock_client.get_product.return_value = {
            "product_id": "BTC-USD",
            "base_currency": "BTC",
            "quote_currency": "USD",
            "status": "online"
        }
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_symbol_info("BTC-USD")
        
        assert result is not None
        assert result["product_id"] == "BTC-USD"
        mock_client.get_product.assert_called_once_with("BTC-USD")
    
    def test_get_symbol_info_not_found(self):
        """Test symbol info retrieval for non-existent symbol."""
        mock_client = Mock()
        mock_client.get_product.return_value = None
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_symbol_info("INVALID-SYMBOL")
        
        assert result is None
    
    def test_get_current_price_success(self):
        """Test successful current price retrieval."""
        mock_client = Mock()
        mock_client.get_product_ticker.return_value = {"price": "20000.00"}
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_current_price("BTC-USD")
        
        assert result == 20000.00
        mock_client.get_product_ticker.assert_called_once_with("BTC-USD")
    
    def test_get_current_price_no_price(self):
        """Test current price retrieval with no price data."""
        mock_client = Mock()
        mock_client.get_product_ticker.return_value = {}
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_current_price("BTC-USD")
        
        assert result is None
    
    def test_is_symbol_available_online(self):
        """Test symbol availability check for online symbol."""
        client = CoinbaseClient()
        
        with patch.object(client, 'get_symbol_info') as mock_get_info:
            mock_get_info.return_value = {"status": "online"}
            
            result = client.is_symbol_available("BTC-USD")
            
            assert result is True
    
    def test_is_symbol_available_offline(self):
        """Test symbol availability check for offline symbol."""
        client = CoinbaseClient()
        
        with patch.object(client, 'get_symbol_info') as mock_get_info:
            mock_get_info.return_value = {"status": "offline"}
            
            result = client.is_symbol_available("BTC-USD")
            
            assert result is False
    
    def test_is_symbol_available_no_info(self):
        """Test symbol availability check with no symbol info."""
        client = CoinbaseClient()
        
        with patch.object(client, 'get_symbol_info') as mock_get_info:
            mock_get_info.return_value = None
            
            result = client.is_symbol_available("BTC-USD")
            
            assert result is False
    
    def test_get_rate_limit_info_success(self):
        """Test successful rate limit info retrieval."""
        mock_client = Mock()
        mock_client.get_rate_limit.return_value = {"limit": 100, "remaining": 50}
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_rate_limit_info()
        
        assert result is not None
        assert result["limit"] == 100
        assert result["remaining"] == 50
    
    def test_get_rate_limit_info_not_available(self):
        """Test rate limit info retrieval when not available."""
        mock_client = Mock()
        mock_client.get_rate_limit.return_value = None
        
        client = CoinbaseClient()
        client.client = mock_client
        
        result = client.get_rate_limit_info()
        
        assert result is None
    
    def test_is_authenticated_property(self):
        """Test is_authenticated property."""
        client = CoinbaseClient()
        client.client = Mock()
        
        with patch('src.coinbase_client.config') as mock_config:
            mock_config.api_credentials_valid = True
            
            assert client.is_authenticated is True
            
            mock_config.api_credentials_valid = False
            
            assert client.is_authenticated is False
    
    def test_sandbox_mode_property(self):
        """Test sandbox_mode property."""
        client = CoinbaseClient()
        
        with patch('src.coinbase_client.config') as mock_config:
            mock_config.sandbox_mode = True
            
            assert client.sandbox_mode is True
            
            mock_config.sandbox_mode = False
            
            assert client.sandbox_mode is False
