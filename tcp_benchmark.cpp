#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <atomic>

using namespace std;
using namespace std::chrono;

const int NUM_CONCURRENT_BOTS = 100;
const int ORDERS_PER_BOT = 1000;
atomic<int> total_orders_sent{0};

void bot_worker() {
    // 1. Create the TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9090);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    // 2. Connect to the Exchange
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        return;
    }

    char buffer[1024];
    // Read the "Connected to Async HFT Engine" welcome message
    read(sock, buffer, 1024); 

    const char* order = "BUY 15000 100\n";
    int len = strlen(order);

    // 3. The Ping-Pong Stress Loop
    for (int i = 0; i < ORDERS_PER_BOT; i++) {
        send(sock, order, len, 0);
        
        // Wait for the server to say "OK: BUY Queued"
        // This prevents TCP packets from gluing together in the buffer
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, 1024); 
        
        total_orders_sent++;
    }
    close(sock);
}

int main() {
    cout << "=== HFT Network Stress Test ===" << endl;
    cout << "Spawning " << NUM_CONCURRENT_BOTS << " concurrent TCP bots..." << endl;

    vector<thread> bots;
    
    // START THE CLOCK
    auto start_time = high_resolution_clock::now();

    for (int i = 0; i < NUM_CONCURRENT_BOTS; i++) {
        bots.emplace_back(bot_worker);
    }

    for (auto& bot : bots) {
        bot.join();
    }

    // STOP THE CLOCK
    auto end_time = high_resolution_clock::now();
    
    auto duration_ns = duration_cast<nanoseconds>(end_time - start_time).count();
    double duration_sec = duration_ns / 1e9;
    double ops = total_orders_sent / duration_sec;

    cout << "\n=== RESULTS ===" << endl;
    cout << "Total Orders Processed: " << total_orders_sent << endl;
    cout << "Total Time:             " << duration_sec << " seconds" << endl;
    cout << "Network Throughput:     " << (long long)ops << " orders / second" << endl;
    cout << "===================\n" << endl;

    return 0;
}