# Lock-Free High-Frequency Trading (HFT) Matching Engine

An ultra-low-latency, asynchronous limit order book and matching engine built entirely from scratch in C++17. 

This project was developed as an independent systems engineering pursuit to explore the physical limits of hardware-level optimization, cache coherency, and Operating System kernel interactions in algorithmic trading architectures.

## ⚡ Core Architecture & Engineering Decisions

Standard data structures and threading models introduce unacceptable latency spikes in HFT environments due to heap fragmentation, cache misses, and OS-level context switching. This engine bypasses those bottlenecks through three custom subsystems:

### 1. Array-Backed Custom Memory Pool ($O(1)$ Allocation)
Standard `std::list` nodes scatter across physical RAM, causing CPU L1/L2 cache misses and forcing the OS to manage heap fragmentation. 
* **The Solution:** A custom Memory Pool utilizing a fixed-size contiguous array (`std::unique_ptr`). Nodes are linked using `uint32_t` array indices instead of raw memory pointers. 
* **The Result:** $O(1)$ allocation/deallocation, zero OS system calls during the hot loop, and perfect spatial locality for L1 cache hits.

### 2. Lock-Free Circular Ring Buffer (Zero Context Switching)
Traditional `std::mutex` locks force threads to sleep, requiring thousands of nanoseconds for the OS to wake them up (Context Switching).
* **The Solution:** Decoupled the network layer from the matching engine using an LMAX Disruptor-style Ring Buffer. Communication across CPU cores is handled strictly through hardware-level atomic operations (`std::atomic<bool>`, `fetch_add`).
* **The Result:** Producer threads and the Engine thread never sleep, wait, or lock, moving data across physical silicon at near hardware limits.

### 3. Asynchronous kqueue Event Loop (Solving the C10k Problem)
A thread-per-client network model collapses under high traffic due to OS thread thrashing. 
* **The Solution:** Replaced synchronous TCP `accept()` loops with the native macOS `kqueue` Event Loop. A single network thread monitors thousands of non-blocking sockets simultaneously, instantly routing packets to the Ring Buffer the microsecond they hit the network card.
* **The Result:** Scales seamlessly to thousands of concurrent bots with zero thread inflation.

---

## 📊 Benchmarks & Performance Metrics

Benchmarks were run on consumer Apple Silicon hardware (ARM64). Results reflect the physical limits of the macOS TCP/IP stack and local cache coherency protocols.

**Internal Engine Performance (Compute-Bound):**
* **Throughput:** ~12,177,000 orders / second
* **Latency:** ~82 nanoseconds / order (includes cross-core L1 cache synchronization)

**Asynchronous Network Performance (I/O-Bound via TCP Loopback):**
* **Throughput:** ~132,400 orders / second (100 concurrent bots)
* **End-to-End Latency:** ~7.5 microseconds / order (crossing OS Kernel Space to User Space)

---

## 🛠️ Build & Run Instructions

This engine requires a compiler that supports C++17 and the `kqueue` API (macOS/BSD). 

### Compilation
Compile the asynchronous server:
```bash
g++ -std=c++17 -O3 OrderBook.cpp main.cpp -o hft_server_async -pthread
