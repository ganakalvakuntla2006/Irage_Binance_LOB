#pragma once
#include <string>
#include <vector>
#include <atomic>

struct AppConfig {
    std::string venue = "spot";              // spot or usdm
    std::vector<std::string> symbols;         // Extracted symbols like BTCUSDT"
    std::string output_dir = "./output";     // Target destination path
    bool is_replay = false;                  // Toggle for offline replay mode
    std::string replay_input_file = "";      // Source file for replay loop
};

struct RuntimeMetrics {
    std::atomic<uint64_t> messages_processed{0}; 
    std::atomic<uint64_t> dropped_packets{0};    
    std::atomic<uint64_t> parse_errors{0};
    std::atomic<uint64_t> network_reconnects{0};
    std::atomic<uint64_t> rows_written{0};
};