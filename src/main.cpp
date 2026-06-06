#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <filesystem>
#include <csignal>
#include <memory>
#include <fstream>
#include <chrono>
#include <atomic>
#include <thread>
#include "config.hpp"
#include "ring_buffer.hpp"
#include "websocket_client.hpp"
#include "order_book.hpp"
#include <csignal>

// Global flags to control program execution and track performance
std::atomic<bool> g_running{true};
RuntimeMetrics g_metrics;

// Capture system interrupts (Ctrl+C) to trigger shutdown
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) g_running = false;
}

// Turn comma separated input string into a list of uppercase symbols
std::vector<std::string> split_symbols(const std::string& s) {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            for (char &c : item) c = std::toupper(c);
            res.push_back(item);
        }
    }
    return res;
}

// takes raw JSON, processes it, and updates our logs
void process_json_object(const std::string& json, std::ofstream& market_csv, std::ofstream& ob_csv, 
                         const AppConfig& config, OrderBook& ob, uint64_t seq) {
    try {
        // Tag every message with the exact time it was received
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t tsec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
        int32_t tnsec = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() % 1000000000;

        // Clean up the JSON to ensure it doesn't break CSV format
        std::string clean_json = "\"";
        for(char c : json) {
            if (c == '\n' || c == '\r' || c == '\0') continue;
            if (c == '"') clean_json += "\"\"";
            else clean_json += c;
        }
        clean_json += "\"";
        
        // Pass the raw data into our OrderBook to keep the internal state current
        ob.update(json);

        // Save everything to the master audit file for later replay/analysis
        market_csv << tsec << "|" << tnsec << "|" << config.venue << "|depth|0|0|" << seq 
                   << "|" << config.symbols[0] << "|" << clean_json << "\n";

        // If the book data changed, save a snapshot so we can track price movement over time
        if (ob.has_changed()) {
            ob_csv << ob.format_snapshot_csv(tsec, tnsec, seq, 1, 'U') << "\n";
            ob.reset_change(); 
        }
        
        g_metrics.messages_processed++;

        // Periodically flush files so we don't lose all data if the program crashes
        if (seq % 100 == 0) {
            market_csv.flush();
            ob_csv.flush();
        }
    } catch (const std::exception& e) {
        // Skip bad data packets and keep the program running
        g_metrics.parse_errors++;
        std::cerr << "PARSE ERROR: " << e.what() << " | Seq: " << seq << std::endl;
    }
}

// reads past data from a file instead of connecting to a live feed
void run_replay(const std::string& input_file, std::shared_ptr<SpscRingBuffer<std::string, 65536>> ring_buffer) {
    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "CRITICAL: Could not open replay file: " << input_file << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); 

    while (std::getline(file, line) && g_running) {
        // Extract the JSON payload from the pipe-separated audit log format
        std::stringstream ss(line);
        std::string segment;
        std::string json_payload;
        for (int i = 0; i < 9; ++i) {
            std::getline(ss, segment, '|');
            if (i == 8) json_payload = segment;
        }

        // Undo the CSV escaping we applied earlier
        std::string raw_json;
        for (size_t i = 0; i < json_payload.size(); ++i) {
            if (json_payload[i] == '"' && i + 1 < json_payload.size() && json_payload[i+1] == '"') {
                raw_json += '"';
                i++;
            } else {
                raw_json += json_payload[i];
            }
        }

        // Put the data into the processing queue
        while (!ring_buffer->push(raw_json) && g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    std::cout << "Replay complete." << std::endl;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse command line arguments to configure where to connect and save data
    AppConfig config;
    int duration_limit = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--venue" && i + 1 < argc) config.venue = argv[++i];
        else if (arg == "--symbols" && i + 1 < argc) config.symbols = split_symbols(argv[++i]);
        else if (arg == "--output-dir" && i + 1 < argc) config.output_dir = argv[++i];
        else if (arg == "--duration" && i + 1 < argc) duration_limit = std::stoi(argv[++i]);
        else if (arg == "--replay" && i + 1 < argc) {
            config.is_replay = true;
            config.replay_input_file = argv[++i];
        }
    }

    if (config.symbols.empty()) return 1;
    std::filesystem::create_directories(config.output_dir);
    
    // Prepare files for recording incoming data
    std::ofstream market_csv(config.output_dir + "/market_data_" + config.venue + "_" + config.symbols[0] + ".csv");
    std::ofstream ob_csv(config.output_dir + "/" + config.symbols[0] + "_orderbook.csv");

    market_csv << "recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json\n";
    ob_csv << "tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4\n";

    // Setup the data buffer and connection
    auto ring_buffer = std::make_shared<SpscRingBuffer<std::string, 65536>>();
    WebSocketClient client(config, ring_buffer, g_metrics, g_running);
    OrderBook ob;
    std::thread worker;
    
    // Pick between live mode or data replay mode
    if (config.is_replay) {
        std::cout << "Starting in REPLAY mode..." << std::endl;
        worker = std::thread(run_replay, config.replay_input_file, ring_buffer);
    } else {
        client.start();
    }

    

    uint64_t seq_no = 0;
    std::string raw_message;
    auto start_time = std::chrono::steady_clock::now();

    //constantly drain the buffer until told to stop
    while (g_running || !ring_buffer->is_empty()) {

        // Auto-shutdown if the user specified a maximum duration
        if (duration_limit > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            if (elapsed.count() >= duration_limit) {
                std::cout << "Duration limit reached. Initiating graceful shutdown." << std::endl;
                g_running = false;
            }
        }
        
        // Grab data from the buffer and split it if multiple JSON objects arrived at once
        if (ring_buffer->pop(raw_message)) {
            size_t start = 0;
            size_t end = 0;
            while ((end = raw_message.find("}{", start)) != std::string::npos) {
                process_json_object(raw_message.substr(start, end - start + 1), market_csv, ob_csv, config, ob, seq_no++);
            }
            process_json_object(raw_message.substr(start), market_csv, ob_csv, config, ob, seq_no++);
        } else if (!g_running) {
            break; 
        } else {
            // Briefly pause if buffer is empty to save CPU cycles
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }

    // Cleanup and summary stats
    if (config.is_replay) {
        if (worker.joinable()) worker.join();
    } else {
        client.stop();
    }
    market_csv.close();
    ob_csv.close();

    std::cout << "\nShutdown complete." << std::endl;
    std::cout << "Messages Processed: " << g_metrics.messages_processed << std::endl;
    std::cout << "Dropped Packets:    " << g_metrics.dropped_packets << std::endl;
    std::cout << "Parse Errors:       " << g_metrics.parse_errors << std::endl;
    return 0;
}