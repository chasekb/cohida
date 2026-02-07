#include <iostream>
#include <database/DatabaseManager.h>
#include <data/DataRetriever.h>
#include <config/Config.h>
#include <utils/Logger.h>
#include <models/DataPoint.h>

using namespace std;
using namespace database;
using namespace data;
using namespace config;
using namespace utils;
using namespace models;

int main() {
    try {
        // Initialize logger and configuration
        Logger::initialize("test_database.log");
        Config::get_instance().load(".env");
        
        cout << "=== Database Manager Test ===" << endl;
        
        // Test 1: Database manager initialization
        cout << "1. Testing DatabaseManager initialization..." << endl;
        DatabaseManager db_manager;
        cout << "   ✓ DatabaseManager initialized successfully" << endl;
        
        // Test 2: Connection test
