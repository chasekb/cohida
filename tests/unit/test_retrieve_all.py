"""
Unit tests for the retrieve_all_historical_data functionality.
Tests the complete historical data retrieval with chunking.
"""

import pytest
from datetime import datetime, timedelta
from unittest.mock import Mock, patch
from decimal import Decimal

from src.data_retriever import HistoricalDataRetriever
from src.models import DataRetrievalResult, CryptoPriceData


class TestRetrieveAllHistoricalData:
    """Test cases for retrieve_all_historical_data method."""
    
    @pytest.fixture
    def mock_client(self):
        """Mock Coinbase client."""
        client = Mock()
        client.is_authenticated = True
        client.is_symbol_available.return_value = True
        return client
    
    @pytest.fixture
    def sample_chunk_data(self):
        """Sample data for chunk testing."""
        return [
            CryptoPriceData(
                symbol="BTC-USD",
                timestamp=datetime(2023, 1, 1, 12, 0, 0),
                open_price=Decimal("20000.00"),
                high_price=Decimal("21000.00"),
                low_price=Decimal("19500.00"),
                close_price=Decimal("20500.00"),
                volume=Decimal("1000.50")
            ),
            CryptoPriceData(
                symbol="BTC-USD",
                timestamp=datetime(2023, 1, 1, 13, 0, 0),
                open_price=Decimal("20500.00"),
                high_price=Decimal("21500.00"),
                low_price=Decimal("20000.00"),
                close_price=Decimal("21000.00"),
                volume=Decimal("1200.75")
            )
        ]
    
    def test_retrieve_all_historical_data_success(self, mock_client, sample_chunk_data):
        """Test successful retrieval of all historical data."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            # Mock the retrieve_historical_data method to return sample data
            with patch.object(retriever, 'retrieve_historical_data') as mock_retrieve:
                mock_result = Mock()
                mock_result.success = True
                mock_result.data_points = sample_chunk_data
                mock_retrieve.return_value = mock_result
                
                # Test with small time range to limit chunks
                result = retriever.retrieve_all_historical_data("BTC-USD", granularity=3600, max_years_back=1)
                
                assert result.success is True
                assert result.symbol == "BTC-USD"
                assert len(result.data_points) > 0
                assert mock_retrieve.called
    
    def test_retrieve_all_historical_data_invalid_symbol(self, mock_client):
        """Test retrieval with invalid symbol."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            result = retriever.retrieve_all_historical_data("INVALID-SYMBOL")
            
            assert result.success is False
            assert "Invalid symbol format" in result.error_message
    
    def test_retrieve_all_historical_data_unavailable_symbol(self, mock_client):
        """Test retrieval with unavailable symbol."""
        mock_client.is_symbol_available.return_value = False
        
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            result = retriever.retrieve_all_historical_data("BTC-USD")
            
            assert result.success is False
            assert "not available for trading" in result.error_message
    
    def test_retrieve_all_historical_data_chunk_failure_handling(self, mock_client):
        """Test handling of chunk failures during retrieval."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            # Mock retrieve_historical_data to fail for some chunks
            with patch.object(retriever, 'retrieve_historical_data') as mock_retrieve:
                # First chunk succeeds, second fails, third succeeds
                success_result = Mock()
                success_result.success = True
                success_result.data_points = [Mock()]
                
                failure_result = Mock()
                failure_result.success = False
                failure_result.error_message = "API error"
                
                mock_retrieve.side_effect = [success_result, failure_result, success_result]
                
                result = retriever.retrieve_all_historical_data("BTC-USD", max_years_back=1)
                
                # Should still succeed overall, but with fewer data points
                assert result.success is True
                assert len(result.data_points) == 2  # Two successful chunks
    
    def test_retrieve_all_historical_data_rate_limiting(self, mock_client, sample_chunk_data):
        """Test that rate limiting delay is applied."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            with patch.object(retriever, 'retrieve_historical_data') as mock_retrieve:
                with patch('time.sleep') as mock_sleep:
                    mock_result = Mock()
                    mock_result.success = True
                    mock_result.data_points = sample_chunk_data
                    mock_retrieve.return_value = mock_result
                    
                    retriever.retrieve_all_historical_data("BTC-USD", max_years_back=1)
                    
                    # Verify sleep was called between chunks
                    assert mock_sleep.called
                    mock_sleep.assert_called_with(0.1)
    
    def test_retrieve_all_historical_data_different_granularities(self, mock_client):
        """Test retrieval with different granularity settings."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            with patch.object(retriever, 'retrieve_historical_data') as mock_retrieve:
                mock_result = Mock()
                mock_result.success = True
                mock_result.data_points = []
                mock_retrieve.return_value = mock_result
                
                # Test different granularities
                granularities = [60, 300, 900, 3600, 21600, 86400]
                
                for granularity in granularities:
                    result = retriever.retrieve_all_historical_data("BTC-USD", granularity=granularity, max_years_back=1)
                    assert result.success is True
    
    def test_retrieve_all_historical_data_max_years_parameter(self, mock_client):
        """Test different max_years_back parameter values."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            with patch.object(retriever, 'retrieve_historical_data') as mock_retrieve:
                mock_result = Mock()
                mock_result.success = True
                mock_result.data_points = []
                mock_retrieve.return_value = mock_result
                
                # Test different max years
                max_years_options = [1, 2, 5, 10]
                
                for max_years in max_years_options:
                    result = retriever.retrieve_all_historical_data("BTC-USD", max_years_back=max_years)
                    assert result.success is True
    
    def test_retrieve_all_historical_data_exception_handling(self, mock_client):
        """Test exception handling during retrieval."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            with patch.object(retriever, 'retrieve_historical_data', side_effect=Exception("Unexpected error")):
                result = retriever.retrieve_all_historical_data("BTC-USD")
                
                assert result.success is False
                assert "Failed to retrieve complete historical data" in result.error_message
    
    def test_retrieve_all_historical_data_empty_result(self, mock_client):
        """Test handling when no data is retrieved."""
        with patch('src.data_retriever.coinbase_client', mock_client):
            retriever = HistoricalDataRetriever()
            
            with patch.object(retriever, 'retrieve_historical_data') as mock_retrieve:
                mock_result = Mock()
                mock_result.success = True
                mock_result.data_points = []  # Empty data
                mock_retrieve.return_value = mock_result
                
                result = retriever.retrieve_all_historical_data("BTC-USD", max_years_back=1)
                
                assert result.success is True
                assert len(result.data_points) == 0
