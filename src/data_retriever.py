"""
Historical data retriever for cryptocurrency data from Coinbase API.
Handles data fetching, validation, transformation, and retry logic.
"""

from datetime import datetime, timedelta
from typing import List, Optional, Dict, Any
from decimal import Decimal
import structlog
from tenacity import retry, stop_after_attempt, wait_exponential, retry_if_exception_type

from coinbase_client import coinbase_client
from models import CryptoPriceData, DataRetrievalRequest, DataRetrievalResult, SymbolValidator
from database import DatabaseManager

logger = structlog.get_logger(__name__)


class HistoricalDataRetriever:
    """Fetches historical cryptocurrency data from Coinbase API."""
    
    def __init__(self):
        """Initialize historical data retriever."""
        self.client = coinbase_client
        self._db_managers = {}  # Cache for granularity-specific database managers
        
        if not self.client.is_authenticated:
            logger.error("Coinbase client not authenticated")
            raise ValueError("Coinbase client authentication required")
    
    def get_database_manager(self, granularity: int) -> DatabaseManager:
        """Get database manager for specific granularity."""
        if granularity not in self._db_managers:
            self._db_managers[granularity] = DatabaseManager(granularity)
        return self._db_managers[granularity]
    
    def retrieve_historical_data(self, request: DataRetrievalRequest) -> DataRetrievalResult:
        """
        Retrieve historical data for a given symbol and date range.
        
        Args:
            request: DataRetrievalRequest with symbol, dates, and granularity
            
        Returns:
            DataRetrievalResult with retrieved data or error information
        """
        logger.info(f"Starting historical data retrieval for {request.symbol}", 
                   start_date=request.start_date, end_date=request.end_date)
        
        try:
            # Validate symbol
            if not SymbolValidator.is_valid_symbol(request.symbol):
                error_msg = f"Invalid symbol format: {request.symbol}"
                logger.error(error_msg)
                return DataRetrievalResult(
                    symbol=request.symbol,
                    success=False,
                    data_points=[],
                    error_message=error_msg
                )
            
            # Check if symbol is available
            if not self.client.is_symbol_available(request.symbol):
                error_msg = f"Symbol {request.symbol} is not available for trading"
                logger.error(error_msg)
                return DataRetrievalResult(
                    symbol=request.symbol,
                    success=False,
                    data_points=[],
                    error_message=error_msg
                )
            
            # Retrieve data from API
            raw_data = self._fetch_data_from_api(request)
            
            if not raw_data:
                error_msg = f"No data retrieved for {request.symbol}"
                logger.warning(error_msg)
                return DataRetrievalResult(
                    symbol=request.symbol,
                    success=False,
                    data_points=[],
                    error_message=error_msg
                )
            
            # Transform and validate data
            data_points = self._transform_api_data(raw_data, request.symbol)
            
            logger.info(f"Successfully retrieved {len(data_points)} data points for {request.symbol}")
            
            return DataRetrievalResult(
                symbol=request.symbol,
                success=True,
                data_points=data_points
            )
            
        except Exception as e:
            error_msg = f"Failed to retrieve historical data for {request.symbol}: {str(e)}"
            logger.error(error_msg, exc_info=True)
            return DataRetrievalResult(
                symbol=request.symbol,
                success=False,
                data_points=[],
                error_message=error_msg
            )
    
    @retry(
        stop=stop_after_attempt(3),
        wait=wait_exponential(multiplier=1, min=2, max=10),
        retry=retry_if_exception_type((ConnectionError, TimeoutError, Exception))
    )
    def _fetch_data_from_api(self, request: DataRetrievalRequest) -> Optional[List[Dict[str, Any]]]:
        """
        Fetch raw data from Coinbase API with retry logic.
        
        Args:
            request: DataRetrievalRequest object
            
        Returns:
            List of raw data dictionaries or None if failed
        """
        try:
            # Convert datetime objects to Unix timestamp strings
            start_time = str(int(request.start_date.timestamp()))
            end_time = str(int(request.end_date.timestamp()))
            
            # Convert granularity from seconds to Coinbase API format
            granularity_map = {
                60: 'ONE_MINUTE',
                300: 'FIVE_MINUTE', 
                900: 'FIFTEEN_MINUTE',
                3600: 'ONE_HOUR',
                21600: 'SIX_HOUR',
                86400: 'ONE_DAY'
            }
            
            granularity_str = granularity_map.get(request.granularity, 'ONE_HOUR')
            
            logger.debug(f"Fetching data from API", 
                        symbol=request.symbol, 
                        start_time=start_time, 
                        end_time=end_time,
                        granularity=granularity_str)
            
            # Call Coinbase API for historical data using the new method
            historical_data = self.client.get_historical_candles(
                symbol=request.symbol,
                start=start_time,
                end=end_time,
                granularity=granularity_str
            )
            
            if historical_data and isinstance(historical_data, list):
                logger.debug(f"Retrieved {len(historical_data)} raw data points from API")
                return historical_data
            else:
                logger.warning("API returned empty or invalid data")
                return None
                
        except Exception as e:
            logger.error(f"API call failed: {e}")
            raise
    
    def _transform_api_data(self, raw_data: List[Dict[str, Any]], symbol: str) -> List[CryptoPriceData]:
        """
        Transform raw API data into CryptoPriceData objects.
        
        Args:
            raw_data: Raw data from Coinbase API
            symbol: Cryptocurrency symbol
            
        Returns:
            List of CryptoPriceData objects
        """
        data_points = []
        
        for item in raw_data:
            try:
                # Parse timestamp (Unix timestamp string)
                timestamp = datetime.fromtimestamp(int(item['start']))
                
                # Parse price data from dictionary format
                low_price = Decimal(str(item['low']))
                high_price = Decimal(str(item['high']))
                open_price = Decimal(str(item['open']))
                close_price = Decimal(str(item['close']))
                volume = Decimal(str(item['volume']))
                
                # Create data point
                data_point = CryptoPriceData(
                    symbol=symbol,
                    timestamp=timestamp,
                    open_price=open_price,
                    high_price=high_price,
                    low_price=low_price,
                    close_price=close_price,
                    volume=volume
                )
                
                data_points.append(data_point)
                
            except (ValueError, KeyError, TypeError) as e:
                logger.warning(f"Failed to parse data point: {item}, error: {e}")
                continue
        
        logger.debug(f"Transformed {len(data_points)} valid data points from {len(raw_data)} raw points")
        return data_points
    
    def retrieve_data_for_date_range(self, symbol: str, start_date: datetime, 
                                   end_date: datetime, granularity: int = 3600) -> DataRetrievalResult:
        """
        Convenience method to retrieve data for a date range.
        
        Args:
            symbol: Cryptocurrency symbol
            start_date: Start date for data retrieval
            end_date: End date for data retrieval
            granularity: Data granularity in seconds (default: 3600 = 1 hour)
            
        Returns:
            DataRetrievalResult object
        """
        request = DataRetrievalRequest(
            symbol=symbol,
            start_date=start_date,
            end_date=end_date,
            granularity=granularity
        )
        
        return self.retrieve_historical_data(request)
    
    def retrieve_recent_data(self, symbol: str, days: int = 7, granularity: int = 3600) -> DataRetrievalResult:
        """
        Retrieve recent data for a symbol.
        
        Args:
            symbol: Cryptocurrency symbol
            days: Number of days to retrieve (default: 7)
            granularity: Data granularity in seconds (default: 3600 = 1 hour)
            
        Returns:
            DataRetrievalResult object
        """
        end_date = datetime.utcnow()
        start_date = end_date - timedelta(days=days)
        
        return self.retrieve_data_for_date_range(symbol, start_date, end_date, granularity)
    
    def get_available_granularities(self) -> List[int]:
        """
        Get list of available granularities for data retrieval.
        
        Returns:
            List of granularity values in seconds
        """
        return [60, 300, 900, 3600, 21600, 86400]  # 1min, 5min, 15min, 1hr, 6hr, 1day
    
    def validate_date_range(self, start_date: datetime, end_date: datetime, granularity: int) -> bool:
        """
        Validate if date range is appropriate for given granularity.
        
        Args:
            start_date: Start date
            end_date: End date
            granularity: Granularity in seconds
            
        Returns:
            True if valid, False otherwise
        """
        duration = (end_date - start_date).total_seconds()
        max_duration = granularity * 300  # Coinbase limit: 300 data points per request
        
        return duration <= max_duration
    
    def retrieve_all_historical_data(self, symbol: str, granularity: int = 3600, 
                                   max_years_back: int = 5) -> DataRetrievalResult:
        """
        Retrieve all available historical data for a symbol by chunking requests.
        
        Args:
            symbol: Cryptocurrency symbol
            granularity: Data granularity in seconds (default: 3600 = 1 hour)
            max_years_back: Maximum years to go back (default: 5)
            
        Returns:
            DataRetrievalResult with all retrieved data
        """
        logger.info(f"Starting complete historical data retrieval for {symbol}", 
                   granularity=granularity, max_years_back=max_years_back)
        
        try:
            # Validate symbol
            if not SymbolValidator.is_valid_symbol(symbol):
                error_msg = f"Invalid symbol format: {symbol}"
                logger.error(error_msg)
                return DataRetrievalResult(
                    symbol=symbol,
                    success=False,
                    data_points=[],
                    error_message=error_msg
                )
            
            # Check if symbol is available
            if not self.client.is_symbol_available(symbol):
                error_msg = f"Symbol {symbol} is not available for trading"
                logger.error(error_msg)
                return DataRetrievalResult(
                    symbol=symbol,
                    success=False,
                    data_points=[],
                    error_message=error_msg
                )
            
            # Calculate date range
            end_date = datetime.utcnow()
            start_date = end_date - timedelta(days=max_years_back * 365)
            
            # Calculate chunk size based on granularity (max 300 data points per request)
            chunk_duration = granularity * 300
            chunk_timedelta = timedelta(seconds=chunk_duration)
            
            all_data_points = []
            current_start = start_date
            chunk_count = 0
            
            logger.info(f"Retrieving data in chunks of {chunk_duration} seconds", 
                       total_duration=(end_date - start_date).total_seconds())
            
            while current_start < end_date:
                chunk_count += 1
                current_end = min(current_start + chunk_timedelta, end_date)
                
                logger.debug(f"Processing chunk {chunk_count}: {current_start} to {current_end}")
                
                # Create request for this chunk
                chunk_request = DataRetrievalRequest(
                    symbol=symbol,
                    start_date=current_start,
                    end_date=current_end,
                    granularity=granularity
                )
                
                # Retrieve data for this chunk
                chunk_result = self.retrieve_historical_data(chunk_request)
                
                if not chunk_result.success:
                    logger.warning(f"Chunk {chunk_count} failed: {chunk_result.error_message}")
                    # Continue with next chunk instead of failing completely
                    current_start = current_end
                    continue
                
                if chunk_result.data_points:
                    all_data_points.extend(chunk_result.data_points)
                    logger.debug(f"Chunk {chunk_count} retrieved {len(chunk_result.data_points)} data points")
                
                # Move to next chunk
                current_start = current_end
                
                # Add small delay to respect rate limits
                import time
                time.sleep(0.1)
            
            logger.info(f"Complete historical data retrieval finished", 
                       total_chunks=chunk_count, total_data_points=len(all_data_points))
            
            return DataRetrievalResult(
                symbol=symbol,
                success=True,
                data_points=all_data_points
            )
            
        except Exception as e:
            error_msg = f"Failed to retrieve complete historical data for {symbol}: {str(e)}"
            logger.error(error_msg, exc_info=True)
            return DataRetrievalResult(
                symbol=symbol,
                success=False,
                data_points=[],
                error_message=error_msg
            )


# Global historical data retriever instance
data_retriever = HistoricalDataRetriever()
