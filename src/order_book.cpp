#include "order_book.hpp"
#include <sstream>
#include <cmath>

OrderBook::OrderBook() {}

int64_t OrderBook::parse_fixed_point(const std::string& value_str) {
    if (value_str.empty()) return 0;
    
    size_t decimal_dot = value_str.find('.');
    if (decimal_dot == std::string::npos) {
        return std::stoll(value_str) * 100000000LL;
    }
    
    std::string whole_units = value_str.substr(0, decimal_dot);
    std::string fractional_digits = value_str.substr(decimal_dot + 1);
    
    while (fractional_digits.length() < 8) fractional_digits += "0";
    if (fractional_digits.length() > 8) fractional_digits = fractional_digits.substr(0, 8);
    
    return std::stoll(whole_units) * 100000000LL + std::stoll(fractional_digits);
}

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
}

void OrderBook::reset_to_partial(const std::vector<DepthLevel>& bids, const std::vector<DepthLevel>& asks) {
    bids_map_.clear();
    asks_map_.clear();
    for (const auto& item : bids) bids_map_[item.price_scaled] = item.size_scaled;
    for (const auto& item : asks) asks_map_[item.price_scaled] = item.size_scaled;
}

std::string OrderBook::format_snapshot_csv(int64_t tsec, int32_t tnsec, uint64_t seq_no, int32_t instrument_id, char event_type) {
    std::stringstream row_stream;
    
    // Prefix: tsec,tnsec,SequenceNumber,InstrumentID,EventType,Side
    row_stream << tsec << "," << tnsec << "," << seq_no << "," << instrument_id << "," << event_type << ",N";

    // Format Top 5 Bids
    auto bid_it = bids_map_.begin();
    std::vector<int64_t> top_bid_p(5, 0), top_bid_s(5, 0);
    for (int i = 0; i < 5 && bid_it != bids_map_.end(); ++i, ++bid_it) {
        top_bid_p[i] = bid_it->first;
        top_bid_s[i] = bid_it->second;
    }
    for (int i = 0; i < 5; ++i) row_stream << "," << top_bid_p[i];
    for (int i = 0; i < 5; ++i) row_stream << "," << top_bid_s[i];

    // Format Top 5 Asks
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