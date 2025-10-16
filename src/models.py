"""
Data models and schemas for cryptocurrency historical data.
Defines structured data representations for API responses and database storage.
"""

from dataclasses import dataclass
from datetime import datetime
from typing import Optional, List, Dict, Any
from decimal import Decimal
import structlog

logger = structlog.get_logger(__name__)


@dataclass
class CryptoPriceData:
    """Represents a single cryptocurrency price data point."""
    
    symbol: str
    timestamp: datetime
    open_price: Decimal
    high_price: Decimal
    low_price: Decimal
    close_price: Decimal
    volume: Decimal
    
    def __post_init__(self):
        """Validate data after initialization."""
        if not self.symbol:
            raise ValueError("Symbol cannot be empty")
        
        if self.open_price <= 0 or self.high_price <= 0 or self.low_price <= 0 or self.close_price <= 0:
            raise ValueError("All prices must be positive")
        
        if self.volume < 0:
            raise ValueError("Volume cannot be negative")
        
        # Validate price relationships
        if self.high_price < max(self.open_price, self.close_price):
            logger.warning(f"High price {self.high_price} is lower than open/close prices for {self.symbol}")
        
        if self.low_price > min(self.open_price, self.close_price):
            logger.warning(f"Low price {self.low_price} is higher than open/close prices for {self.symbol}")
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for database storage."""
        return {
            'symbol': self.symbol,
            'timestamp': self.timestamp,
            'open_price': float(self.open_price),
            'high_price': float(self.high_price),
            'low_price': float(self.low_price),
            'close_price': float(self.close_price),
            'volume': float(self.volume)
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'CryptoPriceData':
        """Create instance from dictionary."""
        return cls(
            symbol=data['symbol'],
            timestamp=data['timestamp'],
            open_price=Decimal(str(data['open_price'])),
            high_price=Decimal(str(data['high_price'])),
            low_price=Decimal(str(data['low_price'])),
            close_price=Decimal(str(data['close_price'])),
            volume=Decimal(str(data['volume']))
        )


@dataclass
class SymbolInfo:
    """Represents cryptocurrency symbol information."""
    
    symbol: str
    base_currency: str
    quote_currency: str
    display_name: str
    status: str
    
    def __post_init__(self):
        """Validate symbol information."""
        if not self.symbol:
            raise ValueError("Symbol cannot be empty")
        
        if not self.base_currency or not self.quote_currency:
            raise ValueError("Base and quote currencies must be specified")
        
        if self.status not in ['online', 'offline', 'delisted']:
            logger.warning(f"Unknown status '{self.status}' for symbol {self.symbol}")


@dataclass
class DataRetrievalRequest:
    """Represents a request for historical data retrieval."""
    
    symbol: str
    start_date: datetime
    end_date: datetime
    granularity: int = 3600  # Default to 1 hour
    skip_validation: bool = False  # Skip date range validation for auto-detection
    
    def __post_init__(self):
        """Validate request parameters."""
        if not self.symbol:
            raise ValueError("Symbol cannot be empty")
        
        if self.start_date >= self.end_date:
            raise ValueError("Start date must be before end date")
        
        if self.granularity not in [60, 300, 900, 3600, 21600, 86400]:
            raise ValueError("Granularity must be one of: 60, 300, 900, 3600, 21600, 86400")
        
        # Validate date range (max 300 data points per request) - skip for auto-detection
        if not self.skip_validation:
            # Use a safety margin below the API hard limit (350) to avoid boundary inclusivity issues
            max_duration = self.granularity * 299
            duration = (self.end_date - self.start_date).total_seconds()
            
            if duration > max_duration:
                raise ValueError(f"Date range too large for granularity {self.granularity}. Max duration: {max_duration} seconds")


@dataclass
class DataRetrievalResult:
    """Represents the result of a data retrieval operation."""
    
    symbol: str
    success: bool
    data_points: List[CryptoPriceData]
    error_message: Optional[str] = None
    retrieved_at: Optional[datetime] = None
    
    def __post_init__(self):
        """Set retrieved_at timestamp if not provided."""
        if self.retrieved_at is None:
            self.retrieved_at = datetime.utcnow()
    
    @property
    def data_count(self) -> int:
        """Return the number of data points retrieved."""
        return len(self.data_points)
    
    @property
    def is_empty(self) -> bool:
        """Check if no data was retrieved."""
        return len(self.data_points) == 0


class SymbolValidator:
    """Validates cryptocurrency symbol formats."""
    
    @classmethod
    def is_valid_symbol(cls, symbol: str) -> bool:
        """Check if symbol is valid."""
        if not symbol:
            return False
        
        # Basic format validation (BASE-QUOTE)
        if '-' not in symbol:
            return False
        
        parts = symbol.split('-')
        if len(parts) != 2:
            return False
        
        base, quote = parts
        if not base or not quote:
            return False
        
        # Additional validation: check if it's a reasonable format
        # Base should be 2-10 characters, quote should be 3 characters (like USD, EUR, etc.)
        if len(base) < 2 or len(base) > 10 or len(quote) != 3:
            return False
        
        # Check if both parts contain only alphanumeric characters
        if not base.isalnum() or not quote.isalnum():
            return False
        
        return True
    
    @classmethod
    def normalize_symbol(cls, symbol: str) -> str:
        """Normalize symbol format."""
        if not symbol:
            raise ValueError("Symbol cannot be empty")
        
        normalized = symbol.upper().strip()
        
        if not cls.is_valid_symbol(normalized):
            logger.warning(f"Symbol '{symbol}' may not be supported")
        
        return normalized


class DatabaseSchema:
    """Database schema definitions and SQL operations."""
    
    @staticmethod
    def get_create_table_sql(table_name: str) -> str:
        """Generate CREATE TABLE SQL with configurable table name."""
        return f"""
        CREATE TABLE IF NOT EXISTS {table_name} (
            id SERIAL PRIMARY KEY,
            symbol VARCHAR(20) NOT NULL,
            timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
            open_price DECIMAL(20, 8) NOT NULL,
            high_price DECIMAL(20, 8) NOT NULL,
            low_price DECIMAL(20, 8) NOT NULL,
            close_price DECIMAL(20, 8) NOT NULL,
            volume DECIMAL(20, 8) NOT NULL,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(symbol, timestamp)
        );
        """
    
    @staticmethod
    def get_create_indexes_sql(table_name: str) -> list:
        """Generate CREATE INDEX SQL with configurable table name."""
        return [
            f"CREATE INDEX IF NOT EXISTS idx_crypto_data_symbol ON {table_name}(symbol);",
            f"CREATE INDEX IF NOT EXISTS idx_crypto_data_timestamp ON {table_name}(timestamp);",
            f"CREATE INDEX IF NOT EXISTS idx_crypto_data_symbol_timestamp ON {table_name}(symbol, timestamp);"
        ]
    
    @staticmethod
    def get_insert_data_sql(table_name: str) -> str:
        """Generate INSERT SQL with configurable table name."""
        return f"""
        INSERT INTO {table_name} (symbol, timestamp, open_price, high_price, low_price, close_price, volume)
        VALUES (%(symbol)s, %(timestamp)s, %(open_price)s, %(high_price)s, %(low_price)s, %(close_price)s, %(volume)s)
        ON CONFLICT (symbol, timestamp) DO UPDATE SET
            open_price = EXCLUDED.open_price,
            high_price = EXCLUDED.high_price,
            low_price = EXCLUDED.low_price,
            close_price = EXCLUDED.close_price,
            volume = EXCLUDED.volume,
            created_at = CURRENT_TIMESTAMP;
        """
    
    @staticmethod
    def get_select_data_sql(table_name: str) -> str:
        """Generate SELECT SQL with configurable table name."""
        return f"""
        SELECT symbol, timestamp, open_price, high_price, low_price, close_price, volume
        FROM {table_name}
        WHERE symbol = %(symbol)s
        AND timestamp BETWEEN %(start_date)s AND %(end_date)s
        ORDER BY timestamp;
        """

    # ---- ML Tables ----
    @staticmethod
    def get_create_ml_tables_sql(schema: str) -> list:
        """Return SQL statements to create ML-related tables."""
        return [
            f"""
            CREATE TABLE IF NOT EXISTS {schema}.ml_features (
                id SERIAL PRIMARY KEY,
                symbol VARCHAR(20) NOT NULL,
                timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
                granularity VARCHAR(10) NOT NULL,
                features JSONB NOT NULL,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(symbol, timestamp, granularity)
            );
            """,
            f"""
            CREATE TABLE IF NOT EXISTS {schema}.ml_predictions (
                id SERIAL PRIMARY KEY,
                symbol VARCHAR(20) NOT NULL,
                timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
                granularity VARCHAR(10) NOT NULL,
                model_id VARCHAR(100) NOT NULL,
                prediction JSONB NOT NULL,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(symbol, timestamp, granularity, model_id)
            );
            """,
            f"""
            CREATE TABLE IF NOT EXISTS {schema}.ml_models (
                id SERIAL PRIMARY KEY,
                model_id VARCHAR(100) UNIQUE NOT NULL,
                model_type VARCHAR(50) NOT NULL,
                version VARCHAR(20) NOT NULL,
                metadata JSONB,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
            );
            """,
            f"""
            CREATE TABLE IF NOT EXISTS {schema}.ml_training_runs (
                id SERIAL PRIMARY KEY,
                model_id VARCHAR(100) NOT NULL,
                started_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                finished_at TIMESTAMP WITH TIME ZONE,
                params JSONB,
                metrics JSONB
            );
            """,
            f"CREATE INDEX IF NOT EXISTS idx_ml_features_symbol_ts ON {schema}.ml_features(symbol, timestamp);",
            f"CREATE INDEX IF NOT EXISTS idx_ml_predictions_symbol_ts ON {schema}.ml_predictions(symbol, timestamp);",
            f"CREATE INDEX IF NOT EXISTS idx_ml_predictions_model ON {schema}.ml_predictions(model_id);",
        ]