#pragma once
#include <thread>
#include <atomic>
#include <memory>
#include <boost/asio/ssl.hpp>
#include "config.hpp"
#include "ring_buffer.hpp"

class WebSocketClient {
public:
    // Setup the client with config, the shared buffer for incoming data, and global tracking vars
    WebSocketClient(const AppConfig& config, 
                    std::shared_ptr<SpscRingBuffer<std::string, 65536> > buffer,
                    RuntimeMetrics& metrics,
                    std::atomic<bool>& running);
    
    ~WebSocketClient();
    
    // Start and stop the network connection
    void start();
    void stop();

private:
    // The background thread function that handles all raw socket communication
    void run_network_loop();

    // Configuration and shared resources
    AppConfig config_;
    std::shared_ptr<SpscRingBuffer<std::string, 65536> > shared_buffer_;
    RuntimeMetrics& metrics_;
    std::atomic<bool>& is_running_;
    
    // Thread management and SSL/TLS security settings
    std::thread network_worker_thread_;
    boost::asio::ssl::context ssl_context_;
};