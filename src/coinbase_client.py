"""
Coinbase API client for authentication and connection management.
Handles API authentication, rate limiting, and connection status validation.
"""

from coinbase_advanced_trader import CoinbaseAdvancedTrader
from coinbase_advanced_trader.config import Config as CoinbaseConfig
from typing import Optional, Dict, Any
import structlog
from tenacity import retry, stop_after_attempt, wait_exponential, retry_if_exception_type

from .config import config

logger = structlog.get_logger(__name__)


class CoinbaseClient:
    """Handles authentication and connection to Coinbase Advanced API."""
    
    def __init__(self):
        """Initialize Coinbase client with authentication credentials."""
        self.client: Optional[CoinbaseAdvancedTrader] = None
        self._authenticate()
    
    def _authenticate(self) -> None:
        """Authenticate with Coinbase API using configured credentials."""
        if not config.api_credentials_valid:
            logger.error("Coinbase API credentials not properly configured")
            raise ValueError("Missing required API credentials")
        
        try:
            # Configure Coinbase client
            coinbase_config = CoinbaseConfig(
                api_key=config.api_key,
                api_secret=config.api_secret,
                passphrase=config.api_passphrase,
                sandbox=config.sandbox_mode
            )
            
            # Initialize client
            self.client = CoinbaseAdvancedTrader(coinbase_config)
            
            logger.info("Coinbase API client authenticated successfully", 
                       sandbox_mode=config.sandbox_mode)
            
        except Exception as e:
            logger.error(f"Failed to authenticate with Coinbase API: {e}")
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
            # Test connection by getting account information
            accounts = self.client.get_accounts()
            
            if accounts and len(accounts) > 0:
                logger.info("Coinbase API connection test successful")
                return True
            else:
                logger.warning("Coinbase API connection test returned empty accounts")
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
            products = self.client.get_products()
            
            if products:
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
            product = self.client.get_product(symbol)
            
            if product:
                logger.debug(f"Retrieved symbol info for {symbol}")
                return product
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
            ticker = self.client.get_product_ticker(symbol)
            
            if ticker and 'price' in ticker:
                price = float(ticker['price'])
                logger.debug(f"Current price for {symbol}: {price}")
                return price
            else:
                logger.warning(f"No price data found for {symbol}")
                return None
                
        except Exception as e:
            logger.error(f"Failed to get current price for {symbol}: {e}")
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
            status = symbol_info.get('status', '').lower()
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
        return self.client is not None and config.api_credentials_valid
    
    @property
    def sandbox_mode(self) -> bool:
        """Check if running in sandbox mode."""
        return config.sandbox_mode


# Global Coinbase client instance
coinbase_client = CoinbaseClient()
