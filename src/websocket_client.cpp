#include "websocket_client.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <iostream>
#include <chrono>
#include <openssl/ssl.h>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp> 
#include <boost/beast/core/tcp_stream.hpp> 
#include <boost/asio/connect.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Initialize components and set SSL to use standard TLS 1.2 for secure connections
WebSocketClient::WebSocketClient(const AppConfig& config, 
                                 std::shared_ptr<SpscRingBuffer<std::string, 65536> > buffer,
                                 RuntimeMetrics& metrics,
                                 std::atomic<bool>& running)
    : config_(config), shared_buffer_(buffer), metrics_(metrics), 
      is_running_(running), ssl_context_(boost::asio::ssl::context::tlsv12_client) {}

WebSocketClient::~WebSocketClient() { stop(); }

void WebSocketClient::start() {
    // Skip complex certificate verification to simplify connection to Binance's public API
    ssl_context_.set_verify_mode(asio::ssl::verify_none);
    // Spin up the background network thread to prevent blocking the main program
    network_worker_thread_ = std::thread(&WebSocketClient::run_network_loop, this);
}

void WebSocketClient::stop() {
    //  clean up the network thread when requested
    if (network_worker_thread_.joinable()) network_worker_thread_.join();
}

void WebSocketClient::run_network_loop() {
    // Keep trying to connect if the program is supposed to be running
    while (is_running_) {
        try {
            // Build the URL endpoint based on user symbols and Binance naming rules
            std::string host = (config_.venue == "usdm") ? "fstream.binance.com" : "stream.binance.com";
            std::string port = "443";
            std::string stream_endpoint = "/ws/";
            for (size_t i = 0; i < config_.symbols.size(); ++i) {
                std::string sym = config_.symbols[i];
                for (char &c : sym) c = std::tolower(c);
                stream_endpoint += sym + "@depth@100ms/" + sym + "@depth5@100ms/" + sym + "@trade";
                if (i + 1 < config_.symbols.size()) stream_endpoint += "/";
            }

            asio::io_context ioc;
            tcp::resolver resolver(ioc);
            
            websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws(ioc, ssl_context_);

            // Resolve host and perform raw TCP handshake
            auto const results = resolver.resolve(host, port);
            beast::get_lowest_layer(ws).connect(results);

            // Perform SSL handshake to encrypt the connection
            SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str());
            ws.next_layer().handshake(asio::ssl::stream_base::client);

            // Set up HTTP headers for the websocket upgrade request
            ws.set_option(websocket::stream_base::decorator([&host](websocket::request_type& req) {
                req.set(beast::http::field::host, host);
                req.set(beast::http::field::user_agent, "HFTCaptureClient/1.0");
            }));

            // Finalize the connection to Binance
            ws.handshake(host, stream_endpoint);

            std::cout << "SUCCESS: Connection established." << std::endl;

            

            beast::flat_buffer buffer;
            // constantly listen for new market data packets
            while (is_running_) {
                // Set a short timeout so we can periodically check if is_running_ has been flipped to false
                beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(2));
                
                try {
                    ws.read(buffer);
                    std::string raw_payload = beast::buffers_to_string(buffer.data());
                    buffer.consume(buffer.size());
                    
                    // Push data to the ring buffer for processing; drop if the consumer is too slow
                    if (!shared_buffer_->push(raw_payload)) {
                        metrics_.dropped_packets++;
                    }
                } catch (const boost::system::system_error& se) {
                    // Ignore harmless timeouts; they allow the loop to re-check the running state
                    if (se.code() == boost::asio::error::operation_aborted || 
                        se.code() == boost::asio::error::timed_out) {
                        continue;
                    }
                    throw;
                }
            }
            // Cleanup: Close the connection before restarting or exiting
            if (ws.is_open()) ws.close(websocket::close_code::normal);
        } catch (const std::exception& e) {
            // If connection fails, wait a bit before retrying to prevent hammering the server
            if (!is_running_) break;
            std::cerr << "CONNECTION ERROR: " << e.what() << std::endl;
            metrics_.network_reconnects++;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}