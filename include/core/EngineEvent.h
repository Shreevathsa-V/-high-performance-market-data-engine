#pragma once

#include "EventType.h"
#include "Usings.h"

struct EngineEvent
{
    std::uint64_t sequenceNumber;
    EventType type;

    OrderId orderId;

    Price price;
    Quantity quantity;
};