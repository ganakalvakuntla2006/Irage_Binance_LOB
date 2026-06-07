#include "order_book.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>

OrderBook::OrderBook() {}

// Converts price/size strings to integers (fixed-point) to avoid floating-point errors
int64_t OrderBook::parse_fixed_point(const std::string& value_str) {
    if (value_str.empty()) return 0;
    
    size_t decimal_dot = value_str.find('.');
    // If no decimal, just multiply by 10^8
    if (decimal_dot == std::string::npos) {
        return std::stoll(value_str) * 100000000LL;
    }
    
    // Split into whole and fractional parts to ensure precision
    std::string whole_units = value_str.substr(0, decimal_dot);
    std::string fractional_digits = value_str.substr(decimal_dot + 1);
    
    // Normalize to 8 decimal places
    while (fractional_digits.length() < 8) fractional_digits += "0";
    if (fractional_digits.length() > 8) fractional_digits = fractional_digits.substr(0, 8);
    
    return std::stoll(whole_units) * 100000000LL + std::stoll(fractional_digits);
}

// Manually extract price/size data from the raw JSON string without heavy libraries
void OrderBook::update(const std::string& json) {
    auto parse_list = [&](const std::string& key, bool is_bid) {
    size_t key_pos = json.find("\"" + key + "\":");
    if (key_pos == std::string::npos) return;
    
    // Find the start of the list [
    size_t list_start = json.find("[", key_pos);
    // Find the end of the list ]
    size_t list_end = json.find("]", list_start); 
    
    // If the list is empty like bids:[]
    if (json.substr(list_start + 1, 1) == "]") return;

    // Scan through the list
    size_t current = list_start;
    while ((current = json.find("[", current + 1)) != std::string::npos && current < list_end) {
        size_t pair_end = json.find("]", current);
        std::string pair = json.substr(current + 1, pair_end - current - 1);
        
        size_t comma = pair.find(",");
        if (comma != std::string::npos) {
            std::string p = pair.substr(0, comma);
            std::string s = pair.substr(comma + 1);
            
            // Clean quotes
            p.erase(std::remove(p.begin(), p.end(), '\"'), p.end());
            s.erase(std::remove(s.begin(), s.end(), '\"'), s.end());
            
            update_level(p, s, is_bid);
        }
        current = pair_end;
    }
};

    parse_list("bids", true);
    parse_list("asks", false);
}

// Update specific price levels in the map; removes the level if size is zero
void OrderBook::update_level(const std::string& price_str, const std::string& size_str, bool is_bid) {
    int64_t price_scaled = parse_fixed_point(price_str);
    int64_t size_scaled = parse_fixed_point(size_str);

    if (is_bid) {
        if (size_scaled == 0) {
            bids_map_.erase(price_scaled);
        } else {
            bids_map_[price_scaled] = size_scaled;
        }
    } else {
        if (size_scaled == 0) {
            asks_map_.erase(price_scaled);
        } else {
            asks_map_[price_scaled] = size_scaled;
        }
    }
    // Flag this update so the main loop knows it's time to write a new CSV row
    mark_changed();
}

// Completely rebuild the order book state, usually from a full snapshot
void OrderBook::reset_to_partial(const std::vector<DepthLevel>& bids, const std::vector<DepthLevel>& asks) {
    bids_map_.clear();
    asks_map_.clear();
    for (const auto& item : bids) bids_map_[item.price_scaled] = item.size_scaled;
    for (const auto& item : asks) asks_map_[item.price_scaled] = item.size_scaled;
    mark_changed();
}

// Serializes the top 5 bid/ask levels into a flat CSV row for logging
std::string OrderBook::format_snapshot_csv(int64_t tsec, int32_t tnsec, uint64_t seq_no, int32_t instrument_id, char event_type) {
    std::stringstream row_stream;
    
    // Write header metadata
    row_stream << tsec << "," << tnsec << "," << seq_no << "," << instrument_id << "," << event_type << ",N";

    // Extract and format the Top 5 Bids (highest prices first due to map sorting)
    auto bid_it = bids_map_.begin();
    std::vector<int64_t> top_bid_p(5, 0), top_bid_s(5, 0);
    for (int i = 0; i < 5 && bid_it != bids_map_.end(); ++i, ++bid_it) {
        top_bid_p[i] = bid_it->first;
        top_bid_s[i] = bid_it->second;
    }
    for (int i = 0; i < 5; ++i) row_stream << "," << top_bid_p[i];
    for (int i = 0; i < 5; ++i) row_stream << "," << top_bid_s[i];

    // Extract and format the Top 5 Asks
    auto ask_it = asks_map_.begin();
    std::vector<int64_t> top_ask_p(5, 0), top_ask_s(5, 0);
    for (int i = 0; i < 5 && ask_it != asks_map_.end(); ++i, ++ask_it) {
        top_ask_p[i] = ask_it->first;
        top_ask_s[i] = ask_it->second;
    }
    for (int i = 0; i < 5; ++i) row_stream << "," << top_ask_p[i];
    for (int i = 0; i < 5; ++i) row_stream << "," << top_ask_s[i];

    

    return row_stream.str();
}