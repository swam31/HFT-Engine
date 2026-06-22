#include "OrderBook.h"
#include <iostream>
#include <vector>
#include <chrono>

using namespace std;
using namespace std::chrono;

// A simple struct to hold our pre-generated test data
struct TestOrder {
    uint64_t id;
    Side side;
    uint64_t price;
    uint32_t qty;
};

int main() {
    OrderBook engine;
    const int NUM_ORDERS = 1000000; // 1 Million Orders
    vector<TestOrder> orders;
    orders.reserve(NUM_ORDERS);

    cout << "=== HFT Engine Benchmark ===" << endl;
    cout << "Pre-generating " << NUM_ORDERS << " orders..." << endl;

    // 1. Pre-generate the data (so we don't accidentally benchmark the random number generator)
    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        // Keep prices tightly packed between 15000 and 15010 to force massive order matching
        uint64_t price = 15000 + (i % 10); 
        uint32_t qty = 100;
        orders.push_back({(uint64_t)i + 1, side, price, qty});
    }

    cout << "Firing orders into the engine..." << endl;

    // 2. START THE CLOCK
    auto start_time = high_resolution_clock::now();

    // 3. THE HOT PATH (What we are actually measuring)
    for (const auto& order : orders) {
        engine.add_order(order.id, order.side, order.price, order.qty);
    }

    // 4. STOP THE CLOCK
    auto end_time = high_resolution_clock::now();

    // 5. Calculate Metrics
    auto duration_ns = duration_cast<nanoseconds>(end_time - start_time).count();
    double duration_sec = duration_ns / 1e9;
    
    double orders_per_second = NUM_ORDERS / duration_sec;
    double avg_latency_ns = (double)duration_ns / NUM_ORDERS;

    cout << "\n=== RESULTS ===" << endl;
    cout << "Total Time:       " << duration_sec << " seconds" << endl;
    cout << "Throughput:       " << (long long)orders_per_second << " orders / second" << endl;
    cout << "Average Latency:  " << avg_latency_ns << " nanoseconds / order" << endl;
    cout << "===================\n" << endl;

    return 0;
}