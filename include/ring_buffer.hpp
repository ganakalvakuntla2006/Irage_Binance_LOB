#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <cstddef>
template <typename T, size_t Capacity>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacity = 65536) 
        : ring_data_(capacity), capacity_(capacity), head_(0), tail_(0) {}
    bool is_empty() const {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    // Pushed by the live network connection reader thread
    bool push(const std::string& raw_payload) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % capacity_;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer overflow safety drop rule
        }
        
        ring_data_[current_tail] = raw_payload;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Popped by the main execution processing thread
    bool pop(std::string& output_payload) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer empty state
        }
        
        output_payload = std::move(ring_data_[current_head]);
        head_.store((current_head + 1) % capacity_, std::memory_order_release);
        return true;
    }

private:
    std::vector<std::string> ring_data_;
    size_t capacity_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};