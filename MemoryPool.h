#pragma once
#include "types.h"
#include <vector>
#include <cstdint>

struct OrderNode{
    Order order;
    uint32_t prev;
    uint32_t next;
};
// A constant to represent a "Null" pointer in our array
const uint32_t NULL_INDEX = 0xFFFFFFFF;

class MemoryPool {
private:
    std::vector<OrderNode> pool;
    uint32_t free_head;
public:
    MemoryPool(size_t capacity=1000000){
        pool.resize(capacity);

        for(size_t i=0;i<capacity-1;i++){
            pool[i].next=i+1;
        }
        pool[capacity-1].next=NULL_INDEX;
        free_head=0;
    }

    uint32_t allocate(const Order& new_order){
        if(free_head==NULL_INDEX)
            return NULL_INDEX;

        uint32_t index = free_head;
        free_head = pool[index].next;

        pool[index].order = new_order;
        pool[index].prev = NULL_INDEX;
        pool[index].next = NULL_INDEX;

        return index;
    }
    void deallocate(uint32_t index){
        pool[index].next=free_head;
        free_head=index;
    }

    OrderNode& get(uint32_t index){
        return pool[index];
    }

};