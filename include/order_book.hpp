#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include "market_types.hpp"

class OrderBook {
public:
    OrderBook();
    
    // Updates internal book maps. If size is zero, purges the price level.
    void update_level(const std::string& price_str, const std::string& size_str, bool is_bid);
    
    // Completely clears existing data and maps a fresh partial snapshot
    void reset_to_partial(const std::vector<DepthLevel>& bids, const std::vector<DepthLevel>& asks);
    
    // Formats valid 26-column CSV snapshot row precisely matching specifications
    std::string format_snapshot_csv(int64_t tsec, int32_t tnsec, uint64_t seq_no, int32_t instrument_id, char event_type);

    // High-performance string parser scaling numeric values to base 10^8 integers
    static int64_t parse_fixed_point(const std::string& value_str);

private:
    // Bids descending (highest price first), Asks ascending (lowest price first)
    std::map<int64_t, int64_t, std::greater<int64_t>> bids_map_;
    std::map<int64_t, int64_t, std::less<int64_t>> asks_map_;
};