#pragma once

#include "Usings.h"

struct OrderModify
{
    OrderId orderId;
    Price price;
    Quantity quantity;
};