#include "OrderBook.h"
#include "RingBuffer.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h> // macOS kqueue API
#include <cstring>

// --- THE GLOBAL STATE ---
OrderBook global_engine;
RingBuffer lock_free_queue(65536); 
std::atomic<uint64_t> next_order_id{1}; 

// Helper to make sockets Non-Blocking
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// --- THE ENGINE CORE (Consumer) ---
void matching_engine_thread() {
    std::cout << "[Engine] Lock-free core spinning..." << std::endl;
    RingNode task;
    while (true) {
        if (lock_free_queue.poll(task)) {
            if (task.command == 1) {
                global_engine.add_order(task.order_id, task.side, task.price, task.qty);
            } else if (task.command == 2) {
                global_engine.cancel_order(task.order_id);
            }
        }
    }
}

int main() {
    // 1. Spawn the Engine Core
    std::thread(matching_engine_thread).detach();

    // 2. Setup the Listening Socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    set_nonblocking(server_fd); // The server must not block!
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(9090);       
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1024); // Increased backlog for high traffic
    
    // 3. Initialize kqueue (The Radar)
    int kq = kqueue();
    if (kq == -1) {
        std::cerr << "Failed to create kqueue." << std::endl;
        return 1;
    }

    // Register the server_fd onto the radar
    struct kevent change_event;
    EV_SET(&change_event, server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    kevent(kq, &change_event, 1, NULL, 0, NULL);

    std::cout << "=== Asynchronous HFT Exchange Live on Port 9090 ===\n";

    // 4. THE EVENT LOOP (Replaces the 1-thread-per-client loop)
    struct kevent event_list[1024]; // Can process 1024 events simultaneously
    char buffer[1024];

    while (true) {
        // kevent blocks until the radar detects activity
        int num_events = kevent(kq, NULL, 0, event_list, 1024, NULL);

        for (int i = 0; i < num_events; i++) {
            int current_fd = event_list[i].ident;

            // CASE A: A client disconnected
            if (event_list[i].flags & EV_EOF) {
                close(current_fd); // Closing the fd automatically removes it from kqueue
            }
            // CASE B: A new bot is trying to connect
            else if (current_fd == server_fd) {
                int client_socket = accept(server_fd, NULL, NULL);
                if (client_socket >= 0) {
                    set_nonblocking(client_socket);
                    
                    // Add the new bot to the radar
                    struct kevent client_event;
                    EV_SET(&client_event, client_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
                    kevent(kq, &client_event, 1, NULL, 0, NULL);
                    
                    std::string welcome = "Connected to Async HFT Engine.\n";
                    send(client_socket, welcome.c_str(), welcome.length(), 0);
                }
            }
            // CASE C: An existing bot sent us an order packet
            else if (event_list[i].filter == EVFILT_READ) {
                memset(buffer, 0, sizeof(buffer));
                int bytes_read = read(current_fd, buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    std::string raw_command(buffer);
                    std::istringstream iss(raw_command);
                    std::vector<std::string> args;
                    std::string word;
                    while (iss >> word) { args.push_back(word); }

                    if (!args.empty()) {
                        std::string response = "ERR: Invalid\n";
                        
                        if (args[0] == "BUY" && args.size() >= 3) {
                            uint64_t price = std::stoull(args[1]);
                            uint32_t qty = std::stoul(args[2]);
                            uint64_t id = next_order_id.fetch_add(1, std::memory_order_relaxed); 

                            lock_free_queue.push_add_order(id, Side::BUY, price, qty);
                            response = "OK: BUY Queued\n";
                        }
                        else if (args[0] == "SELL" && args.size() >= 3) {
                            uint64_t price = std::stoull(args[1]);
                            uint32_t qty = std::stoul(args[2]);
                            uint64_t id = next_order_id.fetch_add(1, std::memory_order_relaxed);

                            lock_free_queue.push_add_order(id, Side::SELL, price, qty);
                            response = "OK: SELL Queued\n";
                        }

                        // Send confirmation back to the bot
                        send(current_fd, response.c_str(), response.length(), 0);
                    }
                }
            }
        }
    }
    return 0;
}