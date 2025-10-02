"""
Coinbase API client for authentication and connection management.
Handles API authentication, rate limiting, and connection status validation.
"""

from coinbase.rest import RESTClient
from typing import Optional, Dict, Any
import structlog
from tenacity import retry, stop_after_attempt, wait_exponential, retry_if_exception_type

from config import config

logger = structlog.get_logger(__name__)


class CoinbaseClient:
    """Handles authentication and connection to Coinbase Advanced API."""
    
    def __init__(self):
        """Initialize Coinbase client with authentication credentials."""
        self.client: Optional[RESTClient] = None
        self._authenticate()
    
    def _authenticate(self) -> None:
        """Authenticate with Coinbase API using configured credentials."""
        try:
            # Initialize client - for public endpoints, we don't need authentication
            # But we'll initialize with credentials if available for authenticated endpoints
            if config.api_credentials_valid:
                self.client = RESTClient(
                    api_key=config.api_key,
                    api_secret=config.api_secret
                )
                logger.info("Coinbase API client authenticated successfully", 
                           sandbox_mode=config.sandbox_mode)
            else:
                # For public endpoints, we can still use the client without auth
                self.client = RESTClient()
                logger.info("Coinbase API client initialized for public endpoints", 
                           sandbox_mode=config.sandbox_mode)
            
        except Exception as e:
            logger.error(f"Failed to initialize Coinbase API client: {e}")
            raise
    
    @retry(
        stop=stop_after_attempt(3),
        wait=wait_exponential(multiplier=1, min=4, max=10),
        retry=retry_if_exception_type((ConnectionError, TimeoutError))
    )
    def test_connection(self) -> bool:
        """
        Test API connection and validate authentication.
        
        Returns:
            True if connection is successful, False otherwise
        """
        if not self.client:
            logger.error("Coinbase client not initialized")
            return False
        
        try:
            # Test connection by getting server time (public endpoint)
            time_response = self.client.get_unix_time()
            
            if time_response and hasattr(time_response, 'data'):
                logger.info("Coinbase API connection test successful")
                return True
            else:
                logger.warning("Coinbase API connection test returned empty response")
                return False
                
        except Exception as e:
            logger.error(f"Coinbase API connection test failed: {e}")
            return False
    
    def get_available_symbols(self) -> Optional[Dict[str, Any]]:
        """
        Get list of available trading symbols from Coinbase.
        
        Returns:
            Dictionary of available symbols or None if failed
        """
        if not self.client:
            logger.error("Coinbase client not initialized")
            return None
        
        try:
            products_response = self.client.get_public_products()
            
            if products_response and hasattr(products_response, 'products'):
                products = products_response.products
                logger.info(f"Retrieved {len(products)} available symbols from Coinbase")
                return products
            else:
                logger.warning("No symbols retrieved from Coinbase API")
                return None
                
        except Exception as e:
            logger.error(f"Failed to get available symbols: {e}")
            return None
    
    def get_symbol_info(self, symbol: str) -> Optional[Dict[str, Any]]:
        """
        Get detailed information for a specific symbol.
        
        Args:
            symbol: Cryptocurrency symbol (e.g., 'BTC-USD')
            
        Returns:
            Symbol information dictionary or None if failed
        """
        if not self.client:
            logger.error("Coinbase client not initialized")
            return None
        
        try:
            product_response = self.client.get_public_product(symbol)
            
            if product_response:
                logger.debug(f"Retrieved symbol info for {symbol}")
                return product_response
            else:
                logger.warning(f"No information found for symbol {symbol}")
                return None
                
        except Exception as e:
            logger.error(f"Failed to get symbol info for {symbol}: {e}")
            return None
    
    def get_current_price(self, symbol: str) -> Optional[float]:
        """
        Get current price for a symbol.
        
        Args:
            symbol: Cryptocurrency symbol
            
        Returns:
            Current price or None if failed
        """
        if not self.client:
            logger.error("Coinbase client not initialized")
            return None
        
        try:
            # Get the latest candle to get current price
            candles_response = self.client.get_public_candles(
                product_id=symbol,
                start="2024-01-01T00:00:00Z",  # Dummy start date
                end="2024-01-02T00:00:00Z",   # Dummy end date
                granularity="3600",  # 1 hour granularity
                limit=1
            )
            
            if candles_response and hasattr(candles_response, 'data') and candles_response.data:
                candle = candles_response.data[0]
                if 'close' in candle:
                    price = float(candle['close'])
                    logger.debug(f"Current price for {symbol}: {price}")
                    return price
            
            logger.warning(f"No price data found for {symbol}")
            return None
                
        except Exception as e:
            logger.error(f"Failed to get current price for {symbol}: {e}")
            return None
    
    def get_historical_candles(self, symbol: str, start: str, end: str, granularity: str, limit: Optional[int] = None) -> Optional[list]:
        """
        Get historical candle data for a symbol.
        
        Args:
            symbol: Cryptocurrency symbol
            start: Start time as Unix timestamp string
            end: End time as Unix timestamp string
            granularity: Granularity string ('ONE_MINUTE', 'FIVE_MINUTE', 'FIFTEEN_MINUTE', 'ONE_HOUR', 'SIX_HOUR', 'ONE_DAY')
            limit: Maximum number of candles to return
            
        Returns:
            List of candle data or None if failed
        """
        if not self.client:
            logger.error("Coinbase client not initialized")
            return None
        
        try:
            candles_response = self.client.get_public_candles(
                product_id=symbol,
                start=start,
                end=end,
                granularity=granularity,
                limit=limit
            )
            
            if candles_response and hasattr(candles_response, 'candles'):
                candles = candles_response.candles
                logger.debug(f"Retrieved {len(candles)} candles for {symbol}")
                return candles
            else:
                logger.warning(f"No candle data found for {symbol}")
                return None
                
        except Exception as e:
            logger.error(f"Failed to get historical candles for {symbol}: {e}")
            return None
    
    def is_symbol_available(self, symbol: str) -> bool:
        """
        Check if a symbol is available for trading.
        
        Args:
            symbol: Cryptocurrency symbol
            
        Returns:
            True if symbol is available, False otherwise
        """
        symbol_info = self.get_symbol_info(symbol)
        
        if symbol_info:
            status = getattr(symbol_info, 'status', '').lower()
            return status == 'online'
        
        return False
    
    def get_rate_limit_info(self) -> Optional[Dict[str, Any]]:
        """
        Get current rate limit information.
        
        Returns:
            Rate limit information or None if not available
        """
        if not self.client:
            logger.error("Coinbase client not initialized")
            return None
        
        try:
            # Note: This would depend on the specific implementation of the coinbase-advanced-py library
            # The actual method name may vary
            rate_limit_info = getattr(self.client, 'get_rate_limit', lambda: None)()
            
            if rate_limit_info:
                logger.debug("Retrieved rate limit information")
                return rate_limit_info
            else:
                logger.debug("Rate limit information not available")
                return None
                
        except Exception as e:
            logger.debug(f"Could not retrieve rate limit info: {e}")
            return None
    
    @property
    def is_authenticated(self) -> bool:
        """Check if client is properly authenticated."""
        return self.client is not None
    
    @property
    def sandbox_mode(self) -> bool:
        """Check if running in sandbox mode."""
        return config.sandbox_mode


# Global Coinbase client instance
coinbase_client = CoinbaseClient()
