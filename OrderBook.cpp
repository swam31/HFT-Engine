#include "OrderBook.h"
#include <iostream>
#include <algorithm>

using std::min;
using std::cout;

void OrderBook::add_order(uint64_t order_id, Side side, uint64_t price, uint32_t qty) {
    if (side == Side::BUY) {
        // 1. Aggressive Execution
        while (qty > 0 && !asks.empty()) {
            auto best_ask = asks.begin();
            if (best_ask->first > price) break;

            LevelList& list = best_ask->second;
            uint32_t current_idx = list.head;

            while (current_idx != NULL_INDEX && qty > 0) {
                OrderNode& resting_node = pool.get(current_idx);
                uint32_t traded_qty = min(qty, resting_node.order.quantity);
                
                qty -= traded_qty;
                resting_node.order.quantity -= traded_qty;

                if (resting_node.order.quantity == 0) {
                    orders.erase(resting_node.order.order_id);
                    uint32_t next_idx = resting_node.next;

                    // Unlink the node (Move Head Forward)
                    list.head = next_idx;
                    if (next_idx != NULL_INDEX) {
                        pool.get(next_idx).prev = NULL_INDEX;
                    } else {
                        list.tail = NULL_INDEX; // List became empty
                    }

                    // INSTANT O(1) DEALLOCATION! No OS interaction.
                    pool.deallocate(current_idx);
                    current_idx = next_idx;
                } else {
                    break;
                }
            }

            if (list.head == NULL_INDEX) {
                asks.erase(best_ask);
            }
        }

        // 2. Passive Resting (Add to Tail of our custom list)
        if (qty > 0) {
            uint32_t new_idx = pool.allocate({order_id, price, qty, side});
            orders[order_id] = new_idx;

            LevelList& list = bids[price];
            if (list.head == NULL_INDEX) {
                list.head = new_idx;
                list.tail = new_idx;
            } else {
                pool.get(list.tail).next = new_idx;
                pool.get(new_idx).prev = list.tail;
                list.tail = new_idx;
            }
        }
    } 
    else {
        // SELL SIDE LOGIC (Exact mirror of BUY side)
        while (qty > 0 && !bids.empty()) {
            auto best_bid = bids.begin();
            if (best_bid->first < price) break;

            LevelList& list = best_bid->second;
            uint32_t current_idx = list.head;

            while (current_idx != NULL_INDEX && qty > 0) {
                OrderNode& resting_node = pool.get(current_idx);
                uint32_t traded_qty = min(qty, resting_node.order.quantity);
                
                qty -= traded_qty;
                resting_node.order.quantity -= traded_qty;

                if (resting_node.order.quantity == 0) {
                    orders.erase(resting_node.order.order_id);
                    uint32_t next_idx = resting_node.next;

                    list.head = next_idx;
                    if (next_idx != NULL_INDEX) {
                        pool.get(next_idx).prev = NULL_INDEX;
                    } else {
                        list.tail = NULL_INDEX;
                    }

                    pool.deallocate(current_idx);
                    current_idx = next_idx;
                } else {
                    break;
                }
            }

            if (list.head == NULL_INDEX) {
                bids.erase(best_bid);
            }
        }

        if (qty > 0) {
            uint32_t new_idx = pool.allocate({order_id, price, qty, side});
            orders[order_id] = new_idx;

            LevelList& list = asks[price];
            if (list.head == NULL_INDEX) {
                list.head = new_idx;
                list.tail = new_idx;
            } else {
                pool.get(list.tail).next = new_idx;
                pool.get(new_idx).prev = list.tail;
                list.tail = new_idx;
            }
        }
    }
}

// O(1) TELEPORTATION CANCEL - USING ARRAY INDICES
void OrderBook::cancel_order(uint64_t order_id) {
    auto lookup_it = orders.find(order_id);
    if (lookup_it == orders.end()) return;

    uint32_t target_idx = lookup_it->second;
    OrderNode& node = pool.get(target_idx);
    uint64_t price = node.order.price;
    Side side = node.order.side;

    // 1. Unlink the Node from the Array-Backed Linked List
    if (node.prev != NULL_INDEX) {
        pool.get(node.prev).next = node.next;
    } else {
        // It was the Head! Move head forward.
        if (side == Side::BUY) bids[price].head = node.next;
        else asks[price].head = node.next;
    }

    if (node.next != NULL_INDEX) {
        pool.get(node.next).prev = node.prev;
    } else {
        // It was the Tail! Move tail backwards.
        if (side == Side::BUY) bids[price].tail = node.prev;
        else asks[price].tail = node.prev;
    }

    // 2. Clean up the Red-Black Tree if the level is empty
    if (side == Side::BUY) {
        if (bids[price].head == NULL_INDEX) bids.erase(price);
    } else {
        if (asks[price].head == NULL_INDEX) asks.erase(price);
    }

    // 3. Free the memory slot!
    pool.deallocate(target_idx);
    orders.erase(lookup_it);
}

void OrderBook::print_book() const {
    // Only used for debugging, can be left mostly alone, but needs iteration update
    cout << "--- HFT BOOK OMITTED FOR BENCHMARK --- \n"; 
}