"""
Unit tests for data models and schemas.
Tests data validation, transformation, and symbol validation.
"""

import pytest
from datetime import datetime
from decimal import Decimal
from src.models import (
    CryptoPriceData, SymbolInfo, DataRetrievalRequest, DataRetrievalResult,
    SymbolValidator, DatabaseSchema
)


class TestCryptoPriceData:
    """Test cases for CryptoPriceData class."""
    
    def test_valid_data_creation(self):
        """Test creating valid CryptoPriceData instance."""
        data = CryptoPriceData(
            symbol="BTC-USD",
            timestamp=datetime(2023, 1, 1, 12, 0, 0),
            open_price=Decimal("20000.00"),
            high_price=Decimal("21000.00"),
            low_price=Decimal("19500.00"),
            close_price=Decimal("20500.00"),
            volume=Decimal("1000.50")
        )
        
        assert data.symbol == "BTC-USD"
        assert data.open_price == Decimal("20000.00")
        assert data.high_price == Decimal("21000.00")
        assert data.low_price == Decimal("19500.00")
        assert data.close_price == Decimal("20500.00")
        assert data.volume == Decimal("1000.50")
    
    def test_empty_symbol_validation(self):
        """Test validation with empty symbol."""
        with pytest.raises(ValueError, match="Symbol cannot be empty"):
            CryptoPriceData(
                symbol="",
                timestamp=datetime(2023, 1, 1, 12, 0, 0),
                open_price=Decimal("20000.00"),
                high_price=Decimal("21000.00"),
                low_price=Decimal("19500.00"),
                close_price=Decimal("20500.00"),
                volume=Decimal("1000.50")
            )
    
    def test_negative_price_validation(self):
        """Test validation with negative prices."""
        with pytest.raises(ValueError, match="All prices must be positive"):
            CryptoPriceData(
                symbol="BTC-USD",
                timestamp=datetime(2023, 1, 1, 12, 0, 0),
                open_price=Decimal("-20000.00"),
                high_price=Decimal("21000.00"),
                low_price=Decimal("19500.00"),
                close_price=Decimal("20500.00"),
                volume=Decimal("1000.50")
            )
    
    def test_negative_volume_validation(self):
        """Test validation with negative volume."""
        with pytest.raises(ValueError, match="Volume cannot be negative"):
            CryptoPriceData(
                symbol="BTC-USD",
                timestamp=datetime(2023, 1, 1, 12, 0, 0),
                open_price=Decimal("20000.00"),
                high_price=Decimal("21000.00"),
                low_price=Decimal("19500.00"),
                close_price=Decimal("20500.00"),
                volume=Decimal("-1000.50")
            )
    
    def test_to_dict_conversion(self):
        """Test conversion to dictionary."""
        data = CryptoPriceData(
            symbol="BTC-USD",
            timestamp=datetime(2023, 1, 1, 12, 0, 0),
            open_price=Decimal("20000.00"),
            high_price=Decimal("21000.00"),
            low_price=Decimal("19500.00"),
            close_price=Decimal("20500.00"),
            volume=Decimal("1000.50")
        )
        
        result = data.to_dict()
        
        assert result['symbol'] == "BTC-USD"
        assert result['timestamp'] == datetime(2023, 1, 1, 12, 0, 0)
        assert result['open_price'] == 20000.00
        assert result['high_price'] == 21000.00
        assert result['low_price'] == 19500.00
        assert result['close_price'] == 20500.00
        assert result['volume'] == 1000.50
    
    def test_from_dict_creation(self):
        """Test creation from dictionary."""
        data_dict = {
            'symbol': 'ETH-USD',
            'timestamp': datetime(2023, 1, 1, 12, 0, 0),
            'open_price': 1500.00,
            'high_price': 1600.00,
            'low_price': 1400.00,
            'close_price': 1550.00,
            'volume': 2000.75
        }
        
        data = CryptoPriceData.from_dict(data_dict)
        
        assert data.symbol == "ETH-USD"
        assert data.open_price == Decimal("1500.00")
        assert data.high_price == Decimal("1600.00")
        assert data.low_price == Decimal("1400.00")
        assert data.close_price == Decimal("1550.00")
        assert data.volume == Decimal("2000.75")


class TestSymbolInfo:
    """Test cases for SymbolInfo class."""
    
    def test_valid_symbol_info_creation(self):
        """Test creating valid SymbolInfo instance."""
        info = SymbolInfo(
            symbol="BTC-USD",
            base_currency="BTC",
            quote_currency="USD",
            display_name="Bitcoin",
            status="online"
        )
        
        assert info.symbol == "BTC-USD"
        assert info.base_currency == "BTC"
        assert info.quote_currency == "USD"
        assert info.display_name == "Bitcoin"
        assert info.status == "online"
    
    def test_empty_symbol_validation(self):
        """Test validation with empty symbol."""
        with pytest.raises(ValueError, match="Symbol cannot be empty"):
            SymbolInfo(
                symbol="",
                base_currency="BTC",
                quote_currency="USD",
                display_name="Bitcoin",
                status="online"
            )
    
    def test_missing_currencies_validation(self):
        """Test validation with missing currencies."""
        with pytest.raises(ValueError, match="Base and quote currencies must be specified"):
            SymbolInfo(
                symbol="BTC-USD",
                base_currency="",
                quote_currency="USD",
                display_name="Bitcoin",
                status="online"
            )


class TestDataRetrievalRequest:
    """Test cases for DataRetrievalRequest class."""
    
    def test_valid_request_creation(self):
        """Test creating valid DataRetrievalRequest instance."""
        request = DataRetrievalRequest(
            symbol="BTC-USD",
            start_date=datetime(2023, 1, 1),
            end_date=datetime(2023, 1, 2),
            granularity=3600
        )
        
        assert request.symbol == "BTC-USD"
        assert request.start_date == datetime(2023, 1, 1)
        assert request.end_date == datetime(2023, 1, 2)
        assert request.granularity == 3600
    
    def test_empty_symbol_validation(self):
        """Test validation with empty symbol."""
        with pytest.raises(ValueError, match="Symbol cannot be empty"):
            DataRetrievalRequest(
                symbol="",
                start_date=datetime(2023, 1, 1),
                end_date=datetime(2023, 1, 2),
                granularity=3600
            )
    
    def test_invalid_date_range_validation(self):
        """Test validation with invalid date range."""
        with pytest.raises(ValueError, match="Start date must be before end date"):
            DataRetrievalRequest(
                symbol="BTC-USD",
                start_date=datetime(2023, 1, 2),
                end_date=datetime(2023, 1, 1),
                granularity=3600
            )
    
    def test_invalid_granularity_validation(self):
        """Test validation with invalid granularity."""
        with pytest.raises(ValueError, match="Granularity must be one of"):
            DataRetrievalRequest(
                symbol="BTC-USD",
                start_date=datetime(2023, 1, 1),
                end_date=datetime(2023, 1, 2),
                granularity=999
            )
    
    def test_date_range_too_large_validation(self):
        """Test validation with date range too large for granularity."""
        with pytest.raises(ValueError, match="Date range too large"):
            DataRetrievalRequest(
                symbol="BTC-USD",
                start_date=datetime(2023, 1, 1),
                end_date=datetime(2023, 12, 31),  # Too large for 1-hour granularity
                granularity=3600
            )


class TestDataRetrievalResult:
    """Test cases for DataRetrievalResult class."""
    
    def test_successful_result_creation(self):
        """Test creating successful DataRetrievalResult."""
        data_points = [
            CryptoPriceData(
                symbol="BTC-USD",
                timestamp=datetime(2023, 1, 1, 12, 0, 0),
                open_price=Decimal("20000.00"),
                high_price=Decimal("21000.00"),
                low_price=Decimal("19500.00"),
                close_price=Decimal("20500.00"),
                volume=Decimal("1000.50")
            )
        ]
        
        result = DataRetrievalResult(
            symbol="BTC-USD",
            success=True,
            data_points=data_points
        )
        
        assert result.symbol == "BTC-USD"
        assert result.success is True
        assert result.data_count == 1
        assert result.is_empty is False
        assert result.error_message is None
        assert result.retrieved_at is not None
    
    def test_failed_result_creation(self):
        """Test creating failed DataRetrievalResult."""
        result = DataRetrievalResult(
            symbol="BTC-USD",
            success=False,
            data_points=[],
            error_message="API error"
        )
        
        assert result.symbol == "BTC-USD"
        assert result.success is False
        assert result.data_count == 0
        assert result.is_empty is True
        assert result.error_message == "API error"


class TestSymbolValidator:
    """Test cases for SymbolValidator class."""
    
    def test_valid_symbols(self):
        """Test validation of valid symbols."""
        valid_symbols = ['BTC-USD', 'ETH-USD', 'ADA-USD']
        
        for symbol in valid_symbols:
            assert SymbolValidator.is_valid_symbol(symbol) is True
    
    def test_invalid_symbols(self):
        """Test validation of invalid symbols."""
        invalid_symbols = ['', 'BTC', 'BTCUSD', 'INVALID-SYMBOL', 'btc-usd']
        
        for symbol in invalid_symbols:
            assert SymbolValidator.is_valid_symbol(symbol) is False
    
    def test_symbol_normalization(self):
        """Test symbol normalization."""
        test_cases = [
            ('btc-usd', 'BTC-USD'),
            ('ETH-USD', 'ETH-USD'),
            ('  ada-usd  ', 'ADA-USD'),
        ]
        
        for input_symbol, expected in test_cases:
            result = SymbolValidator.normalize_symbol(input_symbol)
            assert result == expected
    
    def test_empty_symbol_normalization(self):
        """Test normalization of empty symbol."""
        with pytest.raises(ValueError, match="Symbol cannot be empty"):
            SymbolValidator.normalize_symbol("")


class TestDatabaseSchema:
    """Test cases for DatabaseSchema class."""
    
    def test_create_table_sql_exists(self):
        """Test that CREATE_TABLE_SQL is defined."""
        assert DatabaseSchema.CREATE_TABLE_SQL is not None
        assert "CREATE TABLE" in DatabaseSchema.CREATE_TABLE_SQL
        assert "crypto_historical_data" in DatabaseSchema.CREATE_TABLE_SQL
    
    def test_create_indexes_sql_exists(self):
        """Test that CREATE_INDEXES_SQL is defined."""
        assert DatabaseSchema.CREATE_INDEXES_SQL is not None
        assert len(DatabaseSchema.CREATE_INDEXES_SQL) > 0
        
        for index_sql in DatabaseSchema.CREATE_INDEXES_SQL:
            assert "CREATE INDEX" in index_sql
    
    def test_insert_data_sql_exists(self):
        """Test that INSERT_DATA_SQL is defined."""
        assert DatabaseSchema.INSERT_DATA_SQL is not None
        assert "INSERT INTO" in DatabaseSchema.INSERT_DATA_SQL
        assert "ON CONFLICT" in DatabaseSchema.INSERT_DATA_SQL
    
    def test_select_data_sql_exists(self):
        """Test that SELECT_DATA_SQL is defined."""
        assert DatabaseSchema.SELECT_DATA_SQL is not None
        assert "SELECT" in DatabaseSchema.SELECT_DATA_SQL
        assert "FROM crypto_historical_data" in DatabaseSchema.SELECT_DATA_SQL
