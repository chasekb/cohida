#include <gtest/gtest.h>
#include "api/CoinbaseClient.h"

using namespace api;

TEST(CoinbaseClientAuthTest, InitializationWithCredentials) {
    CoinbaseClient client("test_key", "test_secret", "test_passphrase");
    EXPECT_TRUE(client.is_authenticated());
}

TEST(CoinbaseClientAuthTest, InitializationWithoutCredentials) {
    CoinbaseClient client;
    EXPECT_FALSE(client.is_authenticated());
}

TEST(CoinbaseClientAuthTest, InitializationWithPartialCredentials) {
    CoinbaseClient client1("test_key", "", "test_passphrase");
    EXPECT_FALSE(client1.is_authenticated());
    
    CoinbaseClient client2("", "test_secret", "test_passphrase");
    EXPECT_FALSE(client2.is_authenticated());
}

