#include "OrderBook.h"
#include "RingBuffer.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

using namespace std;
using namespace std::chrono;

struct TestOrder {
    uint64_t id;
    Side side;
    uint64_t price;
    uint32_t qty;
};

OrderBook engine;
RingBuffer lock_free_queue(1048576); // 1 Million slot capacity for the test
std::atomic<int> orders_processed{0};
const int NUM_ORDERS = 1000000;

// The Consumer (Engine Core)
void matching_engine_thread() {
    RingNode task;
    while (orders_processed.load(std::memory_order_relaxed) < NUM_ORDERS) {
        if (lock_free_queue.poll(task)) {
            engine.add_order(task.order_id, task.side, task.price, task.qty);
            orders_processed.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

int main() {
    vector<TestOrder> orders;
    orders.reserve(NUM_ORDERS);

    cout << "=== Lock-Free HFT Benchmark ===" << endl;
    cout << "Pre-generating " << NUM_ORDERS << " orders..." << endl;

    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        uint64_t price = 15000 + (i % 10); 
        orders.push_back({(uint64_t)i + 1, side, price, 100});
    }

    // Spawn the Engine Thread
    std::thread engine_thread(matching_engine_thread);

    cout << "Firing 1 Million orders through the Ring Buffer..." << endl;

    // START THE CLOCK
    auto start_time = high_resolution_clock::now();

    // The Producer (Network Thread Simulator)
    for (const auto& order : orders) {
        lock_free_queue.push_add_order(order.id, order.side, order.price, order.qty);
    }

    // Wait for the Engine to finish processing all 1 Million orders
    engine_thread.join();

    // STOP THE CLOCK
    auto end_time = high_resolution_clock::now();

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