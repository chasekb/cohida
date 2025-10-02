"""
Integration tests for database operations.
Tests PostgreSQL connectivity, data persistence, and write-as-read capability.
"""

import pytest
import psycopg2
from datetime import datetime, timedelta
from decimal import Decimal
from unittest.mock import patch, Mock
import tempfile
import os

from src.database import DatabaseManager
from src.models import CryptoPriceData, DatabaseSchema


class TestDatabaseManager:
    """Integration tests for DatabaseManager class."""
    
    @pytest.fixture
    def mock_config(self):
        """Mock configuration for testing."""
        config = Mock()
        config.db_host = "localhost"
        config.db_port = 5432
        config.db_name = "test_db"
        config.db_user = "test_user"
        config.db_password = "test_password"
        return config
    
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
    
    def test_database_manager_initialization(self, mock_config):
        """Test database manager initialization."""
        with patch('src.database.config', mock_config):
            with patch('psycopg2.pool.SimpleConnectionPool') as mock_pool:
                mock_pool.return_value = Mock()
                
                db_manager = DatabaseManager()
                
                assert db_manager.connection_pool is not None
                mock_pool.assert_called_once()
    
    def test_connection_pool_initialization_failure(self, mock_config):
        """Test connection pool initialization failure."""
        with patch('src.database.config', mock_config):
            with patch('psycopg2.pool.SimpleConnectionPool', side_effect=psycopg2.Error("Connection failed")):
                with pytest.raises(psycopg2.Error):
                    DatabaseManager()
    
    def test_get_connection_context_manager(self, mock_config):
        """Test connection context manager."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_pool.getconn.return_value = mock_conn
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                with db_manager.get_connection() as conn:
                    assert conn == mock_conn
                
                mock_pool.getconn.assert_called_once()
                mock_pool.putconn.assert_called_once_with(mock_conn)
    
    def test_get_connection_exception_handling(self, mock_config):
        """Test connection context manager exception handling."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_pool.getconn.return_value = mock_conn
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                with pytest.raises(Exception):
                    with db_manager.get_connection() as conn:
                        raise Exception("Test error")
                
                mock_conn.rollback.assert_called_once()
                mock_pool.putconn.assert_called_once_with(mock_conn)
    
    def test_write_data_success(self, mock_config, sample_data_points):
        """Test successful data writing."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                result = db_manager.write_data(sample_data_points)
                
                assert result == 2
                mock_cursor.execute.assert_called()
                mock_conn.commit.assert_called_once()
    
    def test_write_data_empty_list(self, mock_config):
        """Test writing empty data list."""
        with patch('src.database.config', mock_config):
            with patch('psycopg2.pool.SimpleConnectionPool'):
                db_manager = DatabaseManager()
                
                result = db_manager.write_data([])
                
                assert result == 0
    
    def test_write_data_partial_failure(self, mock_config, sample_data_points):
        """Test data writing with partial failures."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            # Make first insert fail, second succeed
            mock_cursor.execute.side_effect = [Exception("Insert failed"), None]
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                result = db_manager.write_data(sample_data_points)
                
                assert result == 1  # Only one successful insert
                mock_conn.commit.assert_called_once()
    
    def test_read_data_success(self, mock_config):
        """Test successful data reading."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            # Mock database rows
            mock_rows = [
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
            mock_cursor.fetchall.return_value = mock_rows
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                start_date = datetime(2023, 1, 1)
                end_date = datetime(2023, 1, 2)
                
                result = db_manager.read_data("BTC-USD", start_date, end_date)
                
                assert len(result) == 1
                assert result[0].symbol == "BTC-USD"
                assert result[0].open_price == Decimal("20000.00")
                
                mock_cursor.execute.assert_called_once()
    
    def test_read_data_empty_result(self, mock_config):
        """Test data reading with empty result."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            mock_cursor.fetchall.return_value = []
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                start_date = datetime(2023, 1, 1)
                end_date = datetime(2023, 1, 2)
                
                result = db_manager.read_data("BTC-USD", start_date, end_date)
                
                assert len(result) == 0
    
    def test_get_data_count_success(self, mock_config):
        """Test successful data count retrieval."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            mock_cursor.fetchone.return_value = [42]
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                result = db_manager.get_data_count("BTC-USD")
                
                assert result == 42
                mock_cursor.execute.assert_called_once()
    
    def test_get_latest_timestamp_success(self, mock_config):
        """Test successful latest timestamp retrieval."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            test_timestamp = datetime(2023, 1, 1, 12, 0, 0)
            mock_cursor.fetchone.return_value = [test_timestamp]
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                result = db_manager.get_latest_timestamp("BTC-USD")
                
                assert result == test_timestamp
                mock_cursor.execute.assert_called_once()
    
    def test_get_latest_timestamp_no_data(self, mock_config):
        """Test latest timestamp retrieval with no data."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            mock_cursor.fetchone.return_value = [None]
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                result = db_manager.get_latest_timestamp("BTC-USD")
                
                assert result is None
    
    def test_test_connection_success(self, mock_config):
        """Test successful connection test."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            mock_cursor.fetchone.return_value = [1]
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                result = db_manager.test_connection()
                
                assert result is True
                mock_cursor.execute.assert_called_once_with("SELECT 1")
    
    def test_test_connection_failure(self, mock_config):
        """Test connection test failure."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_pool.getconn.side_effect = psycopg2.Error("Connection failed")
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                result = db_manager.test_connection()
                
                assert result is False
    
    def test_close_connections(self, mock_config):
        """Test closing all connections."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                db_manager.close_connections()
                
                mock_pool.closeall.assert_called_once()
    
    def test_ensure_schema_exists(self, mock_config):
        """Test schema creation and verification."""
        with patch('src.database.config', mock_config):
            mock_pool = Mock()
            mock_conn = Mock()
            mock_cursor = Mock()
            mock_pool.getconn.return_value = mock_conn
            mock_conn.cursor.return_value.__enter__.return_value = mock_cursor
            
            with patch('psycopg2.pool.SimpleConnectionPool', return_value=mock_pool):
                db_manager = DatabaseManager()
                
                # Verify that schema creation SQL was executed
                assert mock_cursor.execute.call_count >= 1
                mock_conn.commit.assert_called_once()
                
                # Check that CREATE TABLE SQL was called
                create_table_calls = [call for call in mock_cursor.execute.call_args_list 
                                    if call[0][0] == DatabaseSchema.CREATE_TABLE_SQL]
                assert len(create_table_calls) == 1
