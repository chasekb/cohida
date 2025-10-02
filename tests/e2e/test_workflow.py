"""
End-to-end tests for the complete application workflow.
Tests full data retrieval, storage, and CLI functionality.
"""

import pytest
import subprocess
import tempfile
import os
from datetime import datetime, timedelta
from unittest.mock import patch, Mock
import json
import csv

from src.cli import cli
from src.data_retriever import HistoricalDataRetriever
from src.database import DatabaseManager
from src.models import DataRetrievalRequest, CryptoPriceData


class TestEndToEndWorkflow:
    """End-to-end tests for complete application workflow."""
    
    @pytest.fixture
    def mock_api_response(self):
        """Mock API response data."""
        return [
            [1672574400, 19500.00, 21000.00, 20000.00, 20500.00, 1000.50],  # [timestamp, low, high, open, close, volume]
            [1672578000, 20000.00, 21500.00, 20500.00, 21000.00, 1200.75]
        ]
    
    @pytest.fixture
    def sample_data_points(self):
        """Sample data points for testing."""
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
    
    def test_data_retrieval_and_storage_workflow(self, mock_api_response, sample_data_points):
        """Test complete data retrieval and storage workflow."""
        # Mock the Coinbase client
        with patch('src.data_retriever.coinbase_client') as mock_client:
            mock_client.is_authenticated = True
            mock_client.is_symbol_available.return_value = True
            mock_client.client.get_product_candles.return_value = mock_api_response
            
            # Mock database manager
            with patch('src.data_retriever.db_manager') as mock_db:
                mock_db.write_data.return_value = 2
                
                # Create data retriever
                retriever = HistoricalDataRetriever()
                
                # Create request
                request = DataRetrievalRequest(
                    symbol="BTC-USD",
                    start_date=datetime(2023, 1, 1),
                    end_date=datetime(2023, 1, 2),
                    granularity=3600
                )
                
                # Retrieve data
                result = retriever.retrieve_historical_data(request)
                
                # Verify results
                assert result.success is True
                assert result.data_count == 2
                assert result.symbol == "BTC-USD"
                
                # Verify API was called
                mock_client.client.get_product_candles.assert_called_once()
                
                # Verify database write was called
                mock_db.write_data.assert_called_once()
    
    def test_cli_retrieve_command(self, mock_api_response, sample_data_points):
        """Test CLI retrieve command functionality."""
        with patch('src.cli.data_retriever') as mock_retriever:
            with patch('src.cli.db_manager') as mock_db:
                with patch('src.cli.coinbase_client') as mock_client:
                    # Setup mocks
                    mock_client.is_authenticated = True
                    mock_client.is_symbol_available.return_value = True
                    
                    mock_result = Mock()
                    mock_result.success = True
                    mock_result.data_count = 2
                    mock_result.data_points = sample_data_points
                    mock_result.is_empty = False
                    mock_result.error_message = None
                    
                    mock_retriever.retrieve_historical_data.return_value = mock_result
                    mock_db.write_data.return_value = 2
                    
                    # Test CLI command
                    with tempfile.TemporaryDirectory() as temp_dir:
                        result = subprocess.run([
                            'python', '-m', 'src.cli', 'retrieve', 'BTC-USD', 
                            '--days', '7', '--output-dir', temp_dir
                        ], capture_output=True, text=True)
                        
                        # Verify command executed successfully
                        assert result.returncode == 0
                        assert "Successfully retrieved 2 data points" in result.stdout
                        assert "Saved 2 data points to database" in result.stdout
    
    def test_cli_read_command(self, sample_data_points):
        """Test CLI read command functionality."""
        with patch('src.cli.db_manager') as mock_db:
            mock_db.read_data.return_value = sample_data_points
            
            with tempfile.TemporaryDirectory() as temp_dir:
                result = subprocess.run([
                    'python', '-m', 'src.cli', 'read', 'BTC-USD',
                    '--output-dir', temp_dir
                ], capture_output=True, text=True)
                
                # Verify command executed successfully
                assert result.returncode == 0
                assert "Found 2 data points in database" in result.stdout
    
    def test_cli_test_command(self):
        """Test CLI test command functionality."""
        with patch('src.cli.coinbase_client') as mock_client:
            with patch('src.cli.db_manager') as mock_db:
                mock_client.test_connection.return_value = True
                mock_db.test_connection.return_value = True
                
                result = subprocess.run([
                    'python', '-m', 'src.cli', 'test'
                ], capture_output=True, text=True)
                
                # Verify command executed successfully
                assert result.returncode == 0
                assert "✅ Coinbase API connection successful" in result.stdout
                assert "✅ Database connection successful" in result.stdout
    
    def test_cli_symbols_command(self):
        """Test CLI symbols command functionality."""
        with patch('src.cli.coinbase_client') as mock_client:
            mock_symbols = [
                {"product_id": "BTC-USD", "quote_currency": "USD", "status": "online"},
                {"product_id": "ETH-USD", "quote_currency": "USD", "status": "online"},
                {"product_id": "ADA-USD", "quote_currency": "USD", "status": "online"}
            ]
            mock_client.get_available_symbols.return_value = mock_symbols
            
            result = subprocess.run([
                'python', '-m', 'src.cli', 'symbols'
            ], capture_output=True, text=True)
            
            # Verify command executed successfully
            assert result.returncode == 0
            assert "Found 3 available symbols" in result.stdout
            assert "BTC-USD (online)" in result.stdout
    
    def test_cli_info_command(self):
        """Test CLI info command functionality."""
        with patch('src.cli.coinbase_client') as mock_client:
            mock_info = {
                "product_id": "BTC-USD",
                "base_currency": "BTC",
                "quote_currency": "USD",
                "display_name": "Bitcoin",
                "status": "online",
                "base_min_size": "0.00000001",
                "base_max_size": "1000.00000000"
            }
            mock_client.get_symbol_info.return_value = mock_info
            
            result = subprocess.run([
                'python', '-m', 'src.cli', 'info', 'BTC-USD'
            ], capture_output=True, text=True)
            
            # Verify command executed successfully
            assert result.returncode == 0
            assert "Symbol: BTC-USD" in result.stdout
            assert "Base Currency: BTC" in result.stdout
            assert "Quote Currency: USD" in result.stdout
    
    def test_csv_output_format(self, sample_data_points):
        """Test CSV output format generation."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as temp_file:
            temp_path = temp_file.name
        
        try:
            # Import the CLI module to access the save function
            from src.cli import _save_to_csv
            
            _save_to_csv(sample_data_points, temp_path)
            
            # Verify CSV file was created and contains correct data
            assert os.path.exists(temp_path)
            
            with open(temp_path, 'r') as f:
                reader = csv.DictReader(f)
                rows = list(reader)
                
                assert len(rows) == 2
                assert rows[0]['symbol'] == 'BTC-USD'
                assert rows[0]['open_price'] == '20000.0'
                assert rows[0]['high_price'] == '21000.0'
                assert rows[0]['low_price'] == '19500.0'
                assert rows[0]['close_price'] == '20500.0'
                assert rows[0]['volume'] == '1000.5'
        
        finally:
            if os.path.exists(temp_path):
                os.unlink(temp_path)
    
    def test_json_output_format(self, sample_data_points):
        """Test JSON output format generation."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as temp_file:
            temp_path = temp_file.name
        
        try:
            # Import the CLI module to access the save function
            from src.cli import _save_to_json
            
            _save_to_json(sample_data_points, temp_path)
            
            # Verify JSON file was created and contains correct data
            assert os.path.exists(temp_path)
            
            with open(temp_path, 'r') as f:
                data = json.load(f)
                
                assert len(data) == 2
                assert data[0]['symbol'] == 'BTC-USD'
                assert data[0]['open_price'] == 20000.0
                assert data[0]['high_price'] == 21000.0
                assert data[0]['low_price'] == 19500.0
                assert data[0]['close_price'] == 20500.0
                assert data[0]['volume'] == 1000.5
        
        finally:
            if os.path.exists(temp_path):
                os.unlink(temp_path)
    
    def test_error_handling_invalid_symbol(self):
        """Test error handling for invalid symbol."""
        with patch('src.cli.data_retriever') as mock_retriever:
            mock_result = Mock()
            mock_result.success = False
            mock_result.error_message = "Invalid symbol format: INVALID"
            mock_result.data_count = 0
            mock_result.is_empty = True
            
            mock_retriever.retrieve_historical_data.return_value = mock_result
            
            result = subprocess.run([
                'python', '-m', 'src.cli', 'retrieve', 'INVALID-SYMBOL'
            ], capture_output=True, text=True)
            
            # Verify error handling
            assert result.returncode == 0  # CLI should handle errors gracefully
            assert "Error: Invalid symbol format: INVALID" in result.stdout
    
    def test_error_handling_api_failure(self):
        """Test error handling for API failure."""
        with patch('src.cli.data_retriever') as mock_retriever:
            mock_result = Mock()
            mock_result.success = False
            mock_result.error_message = "API connection failed"
            mock_result.data_count = 0
            mock_result.is_empty = True
            
            mock_retriever.retrieve_historical_data.return_value = mock_result
            
            result = subprocess.run([
                'python', '-m', 'src.cli', 'retrieve', 'BTC-USD'
            ], capture_output=True, text=True)
            
            # Verify error handling
            assert result.returncode == 0  # CLI should handle errors gracefully
            assert "Error: API connection failed" in result.stdout
    
    def test_write_as_read_capability(self, sample_data_points):
        """Test write-as-read capability in database operations."""
        with patch('src.database.psycopg2.pool.SimpleConnectionPool') as mock_pool_class:
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            
            mock_pool_class.return_value = mock_pool
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            # Mock database responses
            mock_cursor.fetchall.return_value = [
                {
                    'symbol': 'BTC-USD',
                    'timestamp': datetime(2023, 1, 1, 12, 0, 0),
                    'open_price': 20000.00,
                    'high_price': 21000.00,
                    'low_price': 19500.00,
                    'close_price': 20500.00,
                    'volume': 1000.50
                }
            ]
            
            db_manager = DatabaseManager()
            
            # Write data
            write_result = db_manager.write_data(sample_data_points)
            assert write_result == 2
            
            # Read data back
            read_result = db_manager.read_data(
                "BTC-USD", 
                datetime(2023, 1, 1), 
                datetime(2023, 1, 2)
            )
            assert len(read_result) == 1
            assert read_result[0].symbol == "BTC-USD"
            
            # Verify both write and read operations were called
            assert mock_cursor.execute.call_count >= 3  # Write operations + read operation
            mock_conn.commit.assert_called_once()
