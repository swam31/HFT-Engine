// when other files also tell that include types.h, this will ensure it gets added only once
#pragma once
//helps us access unsigned int etc to ensure that our variable takes same memory on eny machine
#include <cstdint>
//user defined datatype, BUY and SELL as only input, also using class ensures it isnt converted to int
enum class Side{
    BUY,SELL
};

struct Order{
    uint64_t order_id;//8bytes
    uint64_t price;// we store it in unsigned long long...150.55 stored as 15055(8bytes)
    uint32_t quantity;//8bytes
    Side side;//4bytes
};