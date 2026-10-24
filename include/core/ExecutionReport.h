#pragma once

#include "Usings.h"

enum class ExecutionStatus
{
    Filled,
    PartiallyFilled,
    Cancelled,
    Rejected
};

struct ExecutionReport
{
    OrderId orderId;
    Price price;
    Quantity executedQuantity;
    Quantity remainingQuantity;
    ExecutionStatus status;
};