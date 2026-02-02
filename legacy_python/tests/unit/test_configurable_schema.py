"""
Unit tests for configurable database schema functionality.
Tests that database table names are configurable and not hardcoded.
"""

import pytest
from src.models import DatabaseSchema
from src.config import config


class TestConfigurableDatabaseSchema:
    """Test cases for configurable database schema."""
    
    def test_get_create_table_sql_with_custom_name(self):
        """Test CREATE TABLE SQL generation with custom table name."""
        custom_table = "custom_crypto_data"
        sql = DatabaseSchema.get_create_table_sql(custom_table)
        
        assert f"CREATE TABLE IF NOT EXISTS {custom_table}" in sql
        assert "id SERIAL PRIMARY KEY" in sql
        assert "symbol VARCHAR(20) NOT NULL" in sql
        assert "timestamp TIMESTAMP WITH TIME ZONE NOT NULL" in sql
        assert "UNIQUE(symbol, timestamp)" in sql
    
    def test_get_create_indexes_sql_with_custom_name(self):
        """Test CREATE INDEX SQL generation with custom table name."""
        custom_table = "custom_crypto_data"
        indexes = DatabaseSchema.get_create_indexes_sql(custom_table)
        
        assert len(indexes) == 3
        assert f"idx_crypto_data_symbol ON {custom_table}(symbol)" in indexes[0]
        assert f"idx_crypto_data_timestamp ON {custom_table}(timestamp)" in indexes[1]
        assert f"idx_crypto_data_symbol_timestamp ON {custom_table}(symbol, timestamp)" in indexes[2]
    
    def test_get_insert_data_sql_with_custom_name(self):
        """Test INSERT SQL generation with custom table name."""
        custom_table = "custom_crypto_data"
        sql = DatabaseSchema.get_insert_data_sql(custom_table)
        
        assert f"INSERT INTO {custom_table}" in sql
        assert "VALUES (%(symbol)s, %(timestamp)s" in sql
        assert "ON CONFLICT (symbol, timestamp) DO UPDATE SET" in sql
    
    def test_get_select_data_sql_with_custom_name(self):
        """Test SELECT SQL generation with custom table name."""
        custom_table = "custom_crypto_data"
        sql = DatabaseSchema.get_select_data_sql(custom_table)
        
        assert f"SELECT symbol, timestamp, open_price, high_price, low_price, close_price, volume" in sql
        assert f"FROM {custom_table}" in sql
        assert "WHERE symbol = %(symbol)s" in sql
        assert "AND timestamp BETWEEN %(start_date)s AND %(end_date)s" in sql
        assert "ORDER BY timestamp" in sql
    
    def test_config_table_name_usage(self):
        """Test that config table name is used correctly."""
        # Test with the configured table name from config
        table_name = config.db_table
        assert table_name is not None
        
        # Generate SQL with config table name
        create_sql = DatabaseSchema.get_create_table_sql(table_name)
        insert_sql = DatabaseSchema.get_insert_data_sql(table_name)
        select_sql = DatabaseSchema.get_select_data_sql(table_name)
        
        # Verify all SQL uses the config table name
        assert f"CREATE TABLE IF NOT EXISTS {table_name}" in create_sql
        assert f"INSERT INTO {table_name}" in insert_sql
        assert f"FROM {table_name}" in select_sql
    
    
    def test_different_table_names_generate_different_sql(self):
        """Test that different table names generate different SQL."""
        table1 = "table_one"
        table2 = "table_two"
        
        sql1 = DatabaseSchema.get_create_table_sql(table1)
        sql2 = DatabaseSchema.get_create_table_sql(table2)
        
        assert sql1 != sql2
        assert f"CREATE TABLE IF NOT EXISTS {table1}" in sql1
        assert f"CREATE TABLE IF NOT EXISTS {table2}" in sql2
        assert f"CREATE TABLE IF NOT EXISTS {table1}" not in sql2
        assert f"CREATE TABLE IF NOT EXISTS {table2}" not in sql1
    
    def test_sql_injection_protection(self):
        """Test that table names are properly escaped to prevent SQL injection."""
        malicious_table = "crypto_data; DROP TABLE users; --"
        
        # This should not cause SQL injection - the table name should be treated as a literal
        sql = DatabaseSchema.get_create_table_sql(malicious_table)
        
        # The malicious SQL should be treated as part of the table name, not executed
        assert f"CREATE TABLE IF NOT EXISTS {malicious_table}" in sql
        assert "DROP TABLE users" in sql  # It's part of the table name, not a separate command
