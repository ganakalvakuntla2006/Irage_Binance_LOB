#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include "market_types.hpp"

class OrderBook {
public:
    OrderBook();
    
    void update_level(const std::string& price_str, const std::string& size_str, bool is_bid);
    void reset_to_partial(const std::vector<DepthLevel>& bids, const std::vector<DepthLevel>& asks);
    std::string format_snapshot_csv(int64_t tsec, int32_t tnsec, uint64_t seq_no, int32_t instrument_id, char event_type);
    static int64_t parse_fixed_point(const std::string& value_str);

    // observability methods for change detection
    bool has_changed() const { return changed_; }
    void reset_change() { changed_ = false; }
    void mark_changed() { changed_ = true; }
    void update(const std::string& json);

private:
    std::map<int64_t, int64_t, std::greater<int64_t>> bids_map_;
    std::map<int64_t, int64_t, std::less<int64_t>> asks_map_;
    bool changed_ = false; // Tracks if the book was modified
};