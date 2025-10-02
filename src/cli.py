"""
Command-line interface for the Coinbase Historical Data Retrieval application.
Provides user interface for symbol input and data retrieval operations.
"""

import click
from datetime import datetime, timedelta
from typing import Optional
import structlog
import csv
import json
from pathlib import Path

from .config import config
from .coinbase_client import coinbase_client
from .data_retriever import data_retriever
from .database import db_manager
from .models import SymbolValidator, DataRetrievalRequest

logger = structlog.get_logger(__name__)


@click.group()
@click.option('--verbose', '-v', is_flag=True, help='Enable verbose logging')
@click.option('--output-dir', '-o', default='outputs', help='Output directory for data files')
def cli(verbose: bool, output_dir: str):
    """Coinbase Historical Data Retrieval Tool."""
    # Configure logging level
    if verbose:
        config.log_level = "DEBUG"
        structlog.configure(
            wrapper_class=structlog.make_filtering_bound_logger(
                structlog.stdlib.INFO if not verbose else structlog.stdlib.DEBUG
            )
        )
    
    # Ensure output directory exists
    Path(output_dir).mkdir(exist_ok=True)
    
    logger.info("Coinbase Historical Data Retrieval Tool started", 
               verbose=verbose, output_dir=output_dir)


@cli.command()
@click.argument('symbol')
@click.option('--start-date', '-s', help='Start date (YYYY-MM-DD)')
@click.option('--end-date', '-e', help='End date (YYYY-MM-DD)')
@click.option('--days', '-d', type=int, help='Number of days to retrieve (alternative to date range)')
@click.option('--granularity', '-g', type=int, default=3600, 
              help='Data granularity in seconds (60, 300, 900, 3600, 21600, 86400)')
@click.option('--output-format', '-f', type=click.Choice(['csv', 'json']), default='csv',
              help='Output format for data files')
@click.option('--save-to-db', is_flag=True, default=True, help='Save data to PostgreSQL database')
def retrieve(symbol: str, start_date: Optional[str], end_date: Optional[str], 
            days: Optional[int], granularity: int, output_format: str, save_to_db: bool):
    """Retrieve historical data for a cryptocurrency symbol."""
    
    # Normalize symbol
    symbol = SymbolValidator.normalize_symbol(symbol)
    
    # Determine date range
    if days:
        end_dt = datetime.utcnow()
        start_dt = end_dt - timedelta(days=days)
    elif start_date and end_date:
        try:
            start_dt = datetime.strptime(start_date, '%Y-%m-%d')
            end_dt = datetime.strptime(end_date, '%Y-%m-%d')
        except ValueError as e:
            click.echo(f"Error: Invalid date format. Use YYYY-MM-DD. {e}")
            return
    else:
        # Default to last 7 days
        end_dt = datetime.utcnow()
        start_dt = end_dt - timedelta(days=7)
    
    click.echo(f"Retrieving data for {symbol} from {start_dt.date()} to {end_dt.date()}")
    
    # Create retrieval request
    request = DataRetrievalRequest(
        symbol=symbol,
        start_date=start_dt,
        end_date=end_dt,
        granularity=granularity
    )
    
    # Retrieve data
    result = data_retriever.retrieve_historical_data(request)
    
    if not result.success:
        click.echo(f"Error: {result.error_message}")
        return
    
    if result.is_empty:
        click.echo("No data retrieved.")
        return
    
    click.echo(f"Successfully retrieved {result.data_count} data points")
    
    # Save to database if requested
    if save_to_db:
        try:
            written_count = db_manager.write_data(result.data_points)
            click.echo(f"Saved {written_count} data points to database")
        except Exception as e:
            click.echo(f"Error saving to database: {e}")
    
    # Save to file
    timestamp_str = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f"{symbol}_{timestamp_str}.{output_format}"
    filepath = Path(config.output_dir) / filename
    
    try:
        if output_format == 'csv':
            _save_to_csv(result.data_points, filepath)
        else:
            _save_to_json(result.data_points, filepath)
        
        click.echo(f"Data saved to {filepath}")
    except Exception as e:
        click.echo(f"Error saving to file: {e}")


@cli.command()
@click.argument('symbol')
@click.option('--granularity', '-g', type=int, default=3600, 
              help='Data granularity in seconds (60, 300, 900, 3600, 21600, 86400)')
@click.option('--max-years', '-y', type=int, default=5, 
              help='Maximum years to go back (default: 5)')
@click.option('--output-format', '-f', type=click.Choice(['csv', 'json']), default='csv',
              help='Output format for data files')
@click.option('--save-to-db', is_flag=True, default=True, help='Save data to PostgreSQL database')
def retrieve_all(symbol: str, granularity: int, max_years: int, output_format: str, save_to_db: bool):
    """Retrieve all available historical data for a symbol."""
    
    # Normalize symbol
    symbol = SymbolValidator.normalize_symbol(symbol)
    
    click.echo(f"Retrieving ALL historical data for {symbol} (up to {max_years} years back)")
    click.echo(f"This may take several minutes due to API rate limits...")
    
    # Retrieve all data
    result = data_retriever.retrieve_all_historical_data(symbol, granularity, max_years)
    
    if not result.success:
        click.echo(f"Error: {result.error_message}")
        return
    
    if result.is_empty:
        click.echo("No data retrieved.")
        return
    
    click.echo(f"Successfully retrieved {result.data_count} data points")
    
    # Save to database if requested
    if save_to_db:
        try:
            written_count = db_manager.write_data(result.data_points)
            click.echo(f"Saved {written_count} data points to database")
        except Exception as e:
            click.echo(f"Error saving to database: {e}")
    
    # Save to file
    timestamp_str = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f"{symbol}_ALL_{timestamp_str}.{output_format}"
    filepath = Path(config.output_dir) / filename
    
    try:
        if output_format == 'csv':
            _save_to_csv(result.data_points, filepath)
        else:
            _save_to_json(result.data_points, filepath)
        
        click.echo(f"Complete historical data saved to {filepath}")
    except Exception as e:
        click.echo(f"Error saving to file: {e}")


@cli.command()
@click.argument('symbol')
@click.option('--start-date', '-s', help='Start date (YYYY-MM-DD)')
@click.option('--end-date', '-e', help='End date (YYYY-MM-DD)')
@click.option('--output-format', '-f', type=click.Choice(['csv', 'json']), default='csv',
              help='Output format for data files')
def read(symbol: str, start_date: Optional[str], end_date: Optional[str], output_format: str):
    """Read historical data from database for a symbol."""
    
    # Normalize symbol
    symbol = SymbolValidator.normalize_symbol(symbol)
    
    # Determine date range
    if start_date and end_date:
        try:
            start_dt = datetime.strptime(start_date, '%Y-%m-%d')
            end_dt = datetime.strptime(end_date, '%Y-%m-%d')
        except ValueError as e:
            click.echo(f"Error: Invalid date format. Use YYYY-MM-DD. {e}")
            return
    else:
        # Default to last 30 days
        end_dt = datetime.utcnow()
        start_dt = end_dt - timedelta(days=30)
    
    click.echo(f"Reading data for {symbol} from {start_dt.date()} to {end_dt.date()}")
    
    try:
        data_points = db_manager.read_data(symbol, start_dt, end_dt)
        
        if not data_points:
            click.echo("No data found in database.")
            return
        
        click.echo(f"Found {len(data_points)} data points in database")
        
        # Save to file
        timestamp_str = datetime.now().strftime('%Y%m%d_%H%M%S')
        filename = f"{symbol}_db_{timestamp_str}.{output_format}"
        filepath = Path(config.output_dir) / filename
        
        if output_format == 'csv':
            _save_to_csv(data_points, filepath)
        else:
            _save_to_json(data_points, filepath)
        
        click.echo(f"Data saved to {filepath}")
        
    except Exception as e:
        click.echo(f"Error reading from database: {e}")


@cli.command()
def test():
    """Test API and database connections."""
    click.echo("Testing connections...")
    
    # Test Coinbase API
    click.echo("Testing Coinbase API connection...")
    if coinbase_client.test_connection():
        click.echo("✅ Coinbase API connection successful")
    else:
        click.echo("❌ Coinbase API connection failed")
    
    # Test database
    click.echo("Testing database connection...")
    if db_manager.test_connection():
        click.echo("✅ Database connection successful")
    else:
        click.echo("❌ Database connection failed")
    
    # Test symbol validation
    click.echo("Testing symbol validation...")
    test_symbols = ['BTC-USD', 'ETH-USD', 'INVALID-SYMBOL']
    for symbol in test_symbols:
        is_valid = SymbolValidator.is_valid_symbol(symbol)
        status = "✅" if is_valid else "❌"
        click.echo(f"{status} {symbol}")


@cli.command()
def symbols():
    """List available cryptocurrency symbols."""
    click.echo("Fetching available symbols from Coinbase...")
    
    try:
        symbols_data = coinbase_client.get_available_symbols()
        
        if symbols_data:
            click.echo(f"Found {len(symbols_data)} available symbols:")
            
            # Filter for USD pairs and display first 20
            usd_symbols = [s for s in symbols_data if s.get('quote_currency') == 'USD']
            usd_symbols = usd_symbols[:20]  # Limit output
            
            for symbol_info in usd_symbols:
                symbol = symbol_info.get('product_id', 'Unknown')
                status = symbol_info.get('status', 'Unknown')
                click.echo(f"  {symbol} ({status})")
            
            if len(usd_symbols) == 20:
                click.echo("  ... (showing first 20 USD pairs)")
        else:
            click.echo("No symbols retrieved from API")
            
    except Exception as e:
        click.echo(f"Error fetching symbols: {e}")


@cli.command()
@click.argument('symbol')
def info(symbol: str):
    """Get detailed information about a symbol."""
    
    # Normalize symbol
    symbol = SymbolValidator.normalize_symbol(symbol)
    
    click.echo(f"Getting information for {symbol}...")
    
    try:
        symbol_info = coinbase_client.get_symbol_info(symbol)
        
        if symbol_info:
            click.echo(f"Symbol: {symbol}")
            click.echo(f"Base Currency: {symbol_info.get('base_currency', 'Unknown')}")
            click.echo(f"Quote Currency: {symbol_info.get('quote_currency', 'Unknown')}")
            click.echo(f"Display Name: {symbol_info.get('display_name', 'Unknown')}")
            click.echo(f"Status: {symbol_info.get('status', 'Unknown')}")
            click.echo(f"Min Size: {symbol_info.get('base_min_size', 'Unknown')}")
            click.echo(f"Max Size: {symbol_info.get('base_max_size', 'Unknown')}")
        else:
            click.echo(f"No information found for {symbol}")
            
    except Exception as e:
        click.echo(f"Error getting symbol info: {e}")


def _save_to_csv(data_points, filepath: Path):
    """Save data points to CSV file."""
    with open(filepath, 'w', newline='') as csvfile:
        fieldnames = ['symbol', 'timestamp', 'open_price', 'high_price', 'low_price', 'close_price', 'volume']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        
        writer.writeheader()
        for data_point in data_points:
            row = data_point.to_dict()
            row['timestamp'] = row['timestamp'].isoformat()
            writer.writerow(row)


def _save_to_json(data_points, filepath: Path):
    """Save data points to JSON file."""
    data = []
    for data_point in data_points:
        row = data_point.to_dict()
        row['timestamp'] = row['timestamp'].isoformat()
        data.append(row)
    
    with open(filepath, 'w') as jsonfile:
        json.dump(data, jsonfile, indent=2)


if __name__ == '__main__':
    cli()
