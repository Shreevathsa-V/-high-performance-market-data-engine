#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "Order.h"
#include "PriceLevel.h"
#include "Usings.h"
#include "Trade.h"
#include "OrderModify.h"
#include "OrderResult.h"
#include "EngineEvent.h"
#include "OrderNodePool.h"
#include "OrderIndex.h"

class OrderBook
{
private:
    using OrderPointer = std::shared_ptr<Order>;

    // Buy side:
    // Highest price comes first.
    std::map<Price, PriceLevel, std::greater<Price>> bids_;

    // Sell side:
    // Lowest price comes first.
    std::map<Price, PriceLevel, std::less<Price>> asks_;

    // Fast lookup by OrderId.
    OrderIndex orderIndex_;

    OrderNodePool orderNodePool_;

    //Event 
    std::uint64_t nextSequenceNumber_{1};
    std::vector<EngineEvent> eventLog_;
    bool eventLoggingEnabled_{true};

    bool CanMatch(Side side, Price price) const;

    bool CanFullyFill(Side side, Price price, Quantity quantity) const;

    void AddOrderToBook(const OrderPointer& order);

    OrderResult MatchOrders(const OrderPointer& incomingOrder);

    void RemoveOrderFromBook(OrderId orderId);

    void AddExecutionReports(std::vector<ExecutionReport>& reports, const OrderPointer& order, Price executionPrice, Quantity executedQuantity) const;

    void RecordEvent(EventType type, OrderId orderId, Price price, Quantity quantity);

public:
    explicit OrderBook(
        bool eventLoggingEnabled = true,
        std::size_t orderPoolCapacity = 100'000,
        std::size_t orderIndexCapacity = 100'000,
        std::size_t orderIndexBucketCount = 262'144)
        : orderIndex_{orderIndexCapacity, orderIndexBucketCount}
        , orderNodePool_{orderPoolCapacity}
        , eventLoggingEnabled_{eventLoggingEnabled}
    {
    }

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;

    ~OrderBook() = default;

    OrderResult AddOrder(const OrderPointer& order);

    bool CancelOrder(OrderId orderId);

    void CancelGoodForDayOrders();

    OrderResult ModifyOrder(const OrderModify& modify);

    std::size_t Size() const noexcept;

    bool Empty() const noexcept;

    Price GetBestBid() const;

    Price GetBestAsk() const;

    Quantity GetBestAskQuantity() const;
    Quantity GetBestBidQuantity() const;

    const std::vector<EngineEvent>& GetEventLog() const noexcept;

    void ReserveEventLog(std::size_t capacity);

    bool ValidateBook() const;
};