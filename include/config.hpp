#pragma once
#include <string>
#include <iostream>

struct RuntimeConfig {
    std::string mode{"live"};              // live for WebSocket streaming, replay for offline files
    std::string symbol{"btcusdt"};         // Target trading pair (lowercase matching Binance convention)
    std::string update_type{"depth5"};     // depth5 (top 5 levels every 250ms) or depth (diff book)
    std::string input_file{""};            // Required if mode is replay
    std::string output_file{"output.csv"}; // Destination for the 26-column metrics layout
    int32_t instrument_id{1};              // Numeric mapping key for evaluation processing

    // Displays clear terminal guidance if invalid flags are passed
    static void print_usage() {
        std::cout << "========================================================================\n"
                  << "Binance LOB Capture Engine - Runtime Usage CLI\n"
                  << "========================================================================\n"
                  << "Flags:\n"
                  << "  --mode <live|replay>       Execution vector (Default: live)\n"
                  << "  --symbol <asset>           Binance pair string (Default: btcusdt)\n"
                  << "  --type <depth5|depth>      Subscription granularity (Default: depth5)\n"
                  << "  --input <path>             Source file path (Mandatory for replay mode)\n"
                  << "  --output <path>            Target destination CSV (Default: output.csv)\n"
                  << "  --id <numeric_id>          Instrument tracking integer (Default: 1)\n"
                  << "========================================================================\n";
    }
};