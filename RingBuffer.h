#pragma once
#include "types.h"
#include <atomic>
#include <memory> // FIX: Required for std::unique_ptr

// The packet of data passed from the Network to the Engine
struct RingNode {
    uint64_t order_id;
    Side side;
    uint64_t price;
    uint32_t qty;
    uint8_t command; // 1 = ADD_ORDER, 2 = CANCEL_ORDER
    
    // The hardware-level flag. True = Network wrote it. False = Engine read it.
    std::atomic<bool> ready{false}; 
};

class RingBuffer {
private:
    // FIX: Replaced std::vector with a raw memory array. Zero overhead, no resize logic.
    std::unique_ptr<RingNode[]> buffer;
    const size_t capacity;
    
    // The 'chasing' pointers
    std::atomic<size_t> write_index{0}; 
    size_t read_index{0}; 

public:
    // FIX: Allocate the exact amount of raw memory on boot
    RingBuffer(size_t size = 65536) : capacity(size), buffer(new RingNode[size]) {}

    // --- CALLED BY NETWORK THREADS ---
    void push_add_order(uint64_t id, Side s, uint64_t p, uint32_t q) {
        size_t idx = write_index.fetch_add(1) % capacity;
        
        buffer[idx].order_id = id;
        buffer[idx].side = s;
        buffer[idx].price = p;
        buffer[idx].qty = q;
        buffer[idx].command = 1;
        
        buffer[idx].ready.store(true, std::memory_order_release);
    }

    void push_cancel_order(uint64_t id) {
        size_t idx = write_index.fetch_add(1) % capacity;
        buffer[idx].order_id = id;
        buffer[idx].command = 2;
        buffer[idx].ready.store(true, std::memory_order_release);
    }

    // --- CALLED BY THE ENGINE THREAD ---
    bool poll(RingNode& out_node) {
        size_t idx = read_index % capacity;
        
        if (!buffer[idx].ready.load(std::memory_order_acquire)) {
            return false; 
        }

        // FIX: Manually copy the data payload so we don't accidentally copy the atomic lock!
        out_node.order_id = buffer[idx].order_id;
        out_node.side     = buffer[idx].side;
        out_node.price    = buffer[idx].price;
        out_node.qty      = buffer[idx].qty;
        out_node.command  = buffer[idx].command;
        
        buffer[idx].ready.store(false, std::memory_order_release);
        
        read_index++;
        return true;
    }
};