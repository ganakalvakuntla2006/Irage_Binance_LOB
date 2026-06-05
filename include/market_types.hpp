#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Fixed-size depth tracking element scaled to 10^8
struct DepthLevel {
    int64_t price_scaled{0};
    int64_t size_scaled{0};
};

// Represents a parsed depth snapshot update event (@depth or @depth5)
struct DepthEvent {
    int64_t recv_tsec{0};
    int32_t recv_tnsec{0};
    uint64_t seq_no{0};
    std::string symbol;
    char event_type{'D'}; // 'D' for differential depth, 'S' for snapshot partial
    
    std::vector<DepthLevel> bids;
    std::vector<DepthLevel> asks;
};

// Tracks a single trade message instance layout (@trade)
struct TradeEvent {
    int64_t recv_tsec{0};
    int32_t recv_tnsec{0};
    uint64_t trade_id{0};
    int64_t price_scaled{0};
    int64_t size_scaled{0};
    std::string symbol;
    bool is_buyer_maker{false};
};