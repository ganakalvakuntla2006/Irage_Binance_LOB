#include <gtest/gtest.h>
#include "../include/order_book.hpp"

TEST(OrderBookTest, HandlesBidUpdate) {
    OrderBook ob;
    
    // Removing all spaces
    std::string json = "{\"bids\":[[\"100.50\",\"10.0\"]],\"asks\":[]}";
    
    ob.update(json);
    
    // Check if the parser actually found the key
    std::cout << "Has changed status: " << (ob.has_changed() ? "Yes" : "No") << std::endl;
    
    std::string output = ob.format_snapshot_csv(123456789, 0, 1, 1, 'U');
    std::cout << "DEBUG OUTPUT: " << output << std::endl; 
    
    EXPECT_TRUE(ob.has_changed()); 
    EXPECT_TRUE(output.find("10050000000") != std::string::npos);
}
// Verify that empty JSON updates don't crash or corrupt the book
TEST(OrderBookTest, HandlesEmptyUpdate) {
    OrderBook ob;
    std::string json = "{\"bids\":[], \"asks\":[]}";
    ob.update(json);
    
    std::string output = ob.format_snapshot_csv(123456789, 0, 1, 1, 'U');
    // The CSV should be full of zeros if the book is empty
    EXPECT_TRUE(output.find("10050000000") == std::string::npos);
}

// Verify that quantity 0.0 effectively removes the price level
TEST(OrderBookTest, HandlesDeleteLevel) {
    OrderBook ob;
    
    // Add a level
    ob.update("{\"bids\":[[\"100.50\", \"10.0\"]],\"asks\":[]}");
    
    // Send update with 0.0 quantity
    ob.update("{\"bids\":[[\"100.50\", \"0.0\"]],\"asks\":[]}");
    
    std::string output = ob.format_snapshot_csv(123456789, 0, 1, 1, 'U');
    
    // 100.50 (scaled) should not be found in the output
    EXPECT_TRUE(output.find("10050000000") == std::string::npos);
    EXPECT_FALSE(ob.has_changed() && output.find("10050000000") != std::string::npos);
}