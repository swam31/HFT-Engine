#pragma once
#include <map>
#include <unordered_map>
#include "types.h"
#include "MemoryPool.h"

// A lightweight struct to track the start and end of our array-backed linked list
struct LevelList {
    uint32_t head = NULL_INDEX;
    uint32_t tail = NULL_INDEX;
};

// Maps price to the Head/Tail indices of the Memory Pool
using BidMap = std::map<uint64_t, LevelList, std::greater<uint64_t>>; 
using AskMap = std::map<uint64_t, LevelList>;

// Maps Order ID to its exact Memory Pool Array Index for O(1) cancels
using OrderMap = std::unordered_map<uint64_t, uint32_t>;

class OrderBook {
    private:
        BidMap bids;
        AskMap asks;
        OrderMap orders;
        
        // ⚠️ THE NEW ENGINE CORE
        // Pre-allocates 1 million slots on startup. Zero allocations during trading.
        MemoryPool pool; 

    public:
        OrderBook() = default;

        void add_order(uint64_t order_id, Side side, uint64_t price, uint32_t qty);
        void cancel_order(uint64_t order_id);
        void print_book() const;
};