"""
PostgreSQL database operations for cryptocurrency historical data.
Handles connection management, schema creation, and data persistence with write-as-read capability.
"""

import psycopg2
from psycopg2.extras import RealDictCursor
from psycopg2.pool import SimpleConnectionPool
from typing import List, Optional, Dict, Any
from datetime import datetime
import structlog
from contextlib import contextmanager

from config import config
from models import CryptoPriceData, DatabaseSchema

logger = structlog.get_logger(__name__)


class DatabaseManager:
    """Manages PostgreSQL database connections and operations."""
    
    def __init__(self):
        """Initialize database manager with connection pool."""
        self.connection_pool: Optional[SimpleConnectionPool] = None
        self._initialize_connection_pool()
        self._ensure_schema_exists()
    
    def _initialize_connection_pool(self) -> None:
        """Initialize connection pool for database operations."""
        try:
            self.connection_pool = SimpleConnectionPool(
                minconn=1,
                maxconn=10,
                host=config.db_host,
                port=config.db_port,
                database=config.db_name,
                user=config.db_user,
                password=config.db_password
            )
            logger.info("Database connection pool initialized successfully")
        except Exception as e:
            logger.error(f"Failed to initialize database connection pool: {e}")
            raise
    
    def _ensure_schema_exists(self) -> None:
        """Ensure database schema and tables exist."""
        try:
            with self.get_connection() as conn:
                with conn.cursor() as cursor:
                    # Create main table using configurable table name
                    create_table_sql = DatabaseSchema.get_create_table_sql(config.db_table)
                    cursor.execute(create_table_sql)
                    
                    # Create indexes using configurable table name
                    index_sqls = DatabaseSchema.get_create_indexes_sql(config.db_table)
                    for index_sql in index_sqls:
                        cursor.execute(index_sql)
                    
                    conn.commit()
                    logger.info("Database schema verified and created if needed", table_name=config.db_table)
        except Exception as e:
            logger.error(f"Failed to ensure schema exists: {e}")
            raise
    
    @contextmanager
    def get_connection(self):
        """Get database connection from pool with automatic cleanup."""
        conn = None
        try:
            conn = self.connection_pool.getconn()
            yield conn
        except Exception as e:
            if conn:
                conn.rollback()
            logger.error(f"Database connection error: {e}")
            raise
        finally:
            if conn:
                self.connection_pool.putconn(conn)
    
    def write_data(self, data_points: List[CryptoPriceData]) -> int:
        """
        Write cryptocurrency data points to database with write-as-read capability.
        
        Args:
            data_points: List of CryptoPriceData objects to write
            
        Returns:
            Number of data points successfully written
        """
        if not data_points:
            logger.warning("No data points provided for writing")
            return 0
        
        written_count = 0
        
        try:
            with self.get_connection() as conn:
                with conn.cursor() as cursor:
                    for data_point in data_points:
                        try:
                            # Convert to dictionary for database insertion
                            data_dict = data_point.to_dict()
                            
                            # Execute insert with conflict resolution using configurable table name
                            insert_sql = DatabaseSchema.get_insert_data_sql(config.db_table)
                            cursor.execute(insert_sql, data_dict)
                            written_count += 1
                            
                            logger.debug(f"Written data point for {data_point.symbol} at {data_point.timestamp}")
                            
                        except Exception as e:
                            logger.error(f"Failed to write data point for {data_point.symbol}: {e}")
                            continue
                    
                    conn.commit()
                    logger.info(f"Successfully wrote {written_count} data points to database")
                    
        except Exception as e:
            logger.error(f"Failed to write data to database: {e}")
            raise
        
        return written_count
    
    def read_data(self, symbol: str, start_date: datetime, end_date: datetime) -> List[CryptoPriceData]:
        """
        Read cryptocurrency data from database for given symbol and date range.
        
        Args:
            symbol: Cryptocurrency symbol (e.g., 'BTC-USD')
            start_date: Start date for data retrieval
            end_date: End date for data retrieval
            
        Returns:
            List of CryptoPriceData objects
        """
        data_points = []
        
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cursor:
                    select_sql = DatabaseSchema.get_select_data_sql(config.db_table)
                    cursor.execute(select_sql, {
                        'symbol': symbol,
                        'start_date': start_date,
                        'end_date': end_date
                    })
                    
                    rows = cursor.fetchall()
                    
                    for row in rows:
                        try:
                            data_point = CryptoPriceData.from_dict(dict(row))
                            data_points.append(data_point)
                        except Exception as e:
                            logger.error(f"Failed to parse data row: {e}")
                            continue
                    
                    logger.info(f"Retrieved {len(data_points)} data points for {symbol}")
                    
        except Exception as e:
            logger.error(f"Failed to read data from database: {e}")
            raise
        
        return data_points
    
    def get_data_count(self, symbol: str) -> int:
        """
        Get total count of data points for a symbol.
        
        Args:
            symbol: Cryptocurrency symbol
            
        Returns:
            Number of data points in database
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor() as cursor:
                    cursor.execute(
                        f"SELECT COUNT(*) FROM {config.db_table} WHERE symbol = %s",
                        (symbol,)
                    )
                    count = cursor.fetchone()[0]
                    logger.debug(f"Data count for {symbol}: {count}")
                    return count
        except Exception as e:
            logger.error(f"Failed to get data count for {symbol}: {e}")
            return 0
    
    def get_latest_timestamp(self, symbol: str) -> Optional[datetime]:
        """
        Get the latest timestamp for a symbol in the database.
        
        Args:
            symbol: Cryptocurrency symbol
            
        Returns:
            Latest timestamp or None if no data exists
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor() as cursor:
                    cursor.execute(
                        f"SELECT MAX(timestamp) FROM {config.db_table} WHERE symbol = %s",
                        (symbol,)
                    )
                    result = cursor.fetchone()[0]
                    logger.debug(f"Latest timestamp for {symbol}: {result}")
                    return result
        except Exception as e:
            logger.error(f"Failed to get latest timestamp for {symbol}: {e}")
            return None
    
    def test_connection(self) -> bool:
        """
        Test database connection.
        
        Returns:
            True if connection is successful, False otherwise
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor() as cursor:
                    cursor.execute("SELECT 1")
                    result = cursor.fetchone()
                    logger.info("Database connection test successful")
                    return result[0] == 1
        except Exception as e:
            logger.error(f"Database connection test failed: {e}")
            return False
    
    def close_connections(self) -> None:
        """Close all database connections."""
        if self.connection_pool:
            self.connection_pool.closeall()
            logger.info("All database connections closed")


# Global database manager instance
db_manager = DatabaseManager()
