#include "OrderBook.h"

#include <stdexcept>
#include <numeric>
#include <chrono>
#include <ctime>
#include <vector>


OrderResult OrderBook::AddOrder(const OrderPointer& order)
{
    if (!order)
        throw std::invalid_argument("Cannot add a null order.");

    const OrderId orderId = order->GetOrderId();

    if (orderIndex_.Contains(orderId))
        return {};

    // FOK must be able to execute completely,
    // otherwise nothing should happen.
    if (order->GetOrderType() == OrderType::FillOrKill)
    {
        if (!CanFullyFill(order->GetSide(), order->GetPrice(), order->GetRemainingQuantity()))
        {
            return {};
        }
    }

    OrderResult result = MatchOrders(order);

    // If the order has been completely filled,
    // there is nothing left to put on the book.
    if (order->IsFilled())
        return result;

    // Immediate orders cannot rest on the book.
    if (order->GetOrderType() == OrderType::FillAndKill || order->GetOrderType() == OrderType::Market)
    {
        return result;
    }

    // GTC and GFD orders with remaining quantity
    // become resting orders.
    AddOrderToBook(order);

    return result;
}

void OrderBook::AddOrderToBook(const OrderPointer& order)
{
    const OrderId orderId = order->GetOrderId();

    if (order->GetSide() == Side::Buy)
    {
        auto [levelIt, inserted] =
            bids_.try_emplace(
                order->GetPrice(),
                order->GetPrice());

        PriceLevel& level = levelIt->second;

        // Get a node from the preallocated pool.
        PriceLevel::Node* node =
            orderNodePool_.Acquire(order);

        // Add the node to the FIFO queue.
        level.AddNode(node);

        orderIndex_.Insert(
            orderId,
            order,
            node);

        RecordEvent(
            EventType::OrderAdded,
            order->GetOrderId(),
            order->GetPrice(),
            order->GetRemainingQuantity());
    }
    else
    {
        auto [levelIt, inserted] =
            asks_.try_emplace(
                order->GetPrice(),
                order->GetPrice());

        PriceLevel& level = levelIt->second;

        // Get a node from the preallocated pool.
        PriceLevel::Node* node =
            orderNodePool_.Acquire(order);

        // Add the node to the FIFO queue.
        level.AddNode(node);

        orderIndex_.Insert(
            orderId,
            order,
            node);

        RecordEvent(
            EventType::OrderAdded,
            order->GetOrderId(),
            order->GetPrice(),
            order->GetRemainingQuantity());
    }
}

void OrderBook::RemoveOrderFromBook(OrderId orderId)
{
    auto* orderEntry = orderIndex_.Find(orderId);

    if (!orderEntry)
    {
        throw std::logic_error("Order not found.");
    }

    OrderPointer order = orderEntry->order;

    if (!order)
    {
        throw std::logic_error(
            "Order book is inconsistent: order pointer is null.");
    }

    const Price price = order->GetPrice();
    PriceLevel::Node* node = orderEntry->location;

    if (!node)
    {
        throw std::logic_error(
            "Order book is inconsistent: node location is null.");
    }

    if (order->GetSide() == Side::Buy)
    {
        auto levelIt = bids_.find(price);

        if (levelIt == bids_.end())
        {
            throw std::logic_error(
                "Order book is inconsistent: bid price level not found.");
        }

        PriceLevel& level = levelIt->second;

        // Remove the node from the FIFO linked list.
        level.RemoveNode(node);

        if (level.Empty())
        {
            bids_.erase(levelIt);
        }
    }
    else
    {
        auto levelIt = asks_.find(price);

        if (levelIt == asks_.end())
        {
            throw std::logic_error(
                "Order book is inconsistent: ask price level not found.");
        }

        PriceLevel& level = levelIt->second;

        // Remove the node from the FIFO linked list.
        level.RemoveNode(node);

        if (level.Empty())
        {
            asks_.erase(levelIt);
        }
    }

    // IMPORTANT:
    // Remove the node from the price level first.
    // Only after that do we return it to the pool.
    orderNodePool_.Release(node);

    orderIndex_.Erase(orderId);
}

bool OrderBook::CancelOrder(OrderId orderId)
{
    auto* orderEntry = orderIndex_.Find(orderId);

    if (!orderEntry)
    {
        return false;
    }

    OrderPointer order = orderEntry->order;

    if (!order)
    {
        throw std::logic_error(
            "Order book is inconsistent: order pointer is null.");
    }

    const Price price = order->GetPrice();
    const Quantity remainingQuantity = order->GetRemainingQuantity();

    RemoveOrderFromBook(orderId);

    RecordEvent(
        EventType::OrderCancelled,
        orderId,
        price,
        remainingQuantity);

    return true;
}

bool OrderBook::CanMatch(
    Side side,
    Price price) const
{
    if (side == Side::Buy)
    {
        if (asks_.empty())
        {
            return false;
        }

        const Price bestAsk =
            asks_.begin()->first;

        return price >= bestAsk;
    }

    if (bids_.empty())
    {
        return false;
    }

    const Price bestBid =
        bids_.begin()->first;

    return price <= bestBid;
}

bool OrderBook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (quantity == 0)
    {
        return true;
    }

    if (side == Side::Buy)
    {
        for (const auto& [levelPrice, level] : asks_)
        {
            if (levelPrice > price)
            {
                break;
            }

            if (level.GetTotalQuantity() >= quantity)
            {
                return true;
            }

            quantity -= level.GetTotalQuantity();
        }
    }
    else
    {
        for (const auto& [levelPrice, level] : bids_)
        {
            if (levelPrice < price)
            {
                break;
            }

            if (level.GetTotalQuantity() >= quantity)
            {
                return true;
            }

            quantity -= level.GetTotalQuantity();
        }
    }

    return false;
}

std::size_t OrderBook::Size() const noexcept //no. of active orders
{
    return orderIndex_.Size();
}

bool OrderBook::Empty() const noexcept
{
    return orderIndex_.Size() == 0;
}

Price OrderBook::GetBestBid() const
{
    if (bids_.empty())
    {
        throw std::logic_error("No bids available.");
    }

    return bids_.begin()->first;
}

Price OrderBook::GetBestAsk() const
{
    if (asks_.empty())
    {
        throw std::logic_error("No asks available.");
    }

    return asks_.begin()->first;
}

OrderResult OrderBook::MatchOrders(const OrderPointer& incomingOrder)
{
    OrderResult result;

    result.reports.reserve(4);
    result.trades.reserve(2);

    if (!incomingOrder)
        return result;

    while (!incomingOrder->IsFilled())
    {
        // =====================================================
        // BUY order matches against ASK orders
        // =====================================================

        if (incomingOrder->GetSide() == Side::Buy)
        {
            if (asks_.empty())
                break;

            auto askLevelIt = asks_.begin();

            const Price bestAsk = askLevelIt->first;

            // Buy order cannot trade above its limit price.
            if (incomingOrder->GetOrderType() != OrderType::Market && incomingOrder->GetPrice() < bestAsk)
            {
                break;
            }

            PriceLevel& askLevel = askLevelIt->second;

            while (!askLevel.Empty() && !incomingOrder->IsFilled())
            {
                // The head node is the oldest order at this price.
                // Therefore FIFO is preserved.
                PriceLevel::Node* askNode =
                    askLevel.GetHead();

                const OrderPointer askOrder = askNode->order;

                const Quantity matchQuantity =std::min(incomingOrder->GetRemainingQuantity(), askOrder->GetRemainingQuantity());

                incomingOrder->Fill(matchQuantity);
                askOrder->Fill(matchQuantity);

                askLevel.ReduceQuantity(matchQuantity);

                AddExecutionReports(
                    result.reports,
                    incomingOrder,
                    bestAsk,
                    matchQuantity);

                AddExecutionReports(
                    result.reports,
                    askOrder,
                    bestAsk,
                    matchQuantity);

                result.trades.push_back(
                    Trade{
                        TradeInfo{
                            incomingOrder->GetOrderId(),
                            bestAsk,
                            matchQuantity
                        },
                        TradeInfo{
                            askOrder->GetOrderId(),
                            bestAsk,
                            matchQuantity
                        }
                    });

                RecordEvent(
                    EventType::TradeExecuted,
                    incomingOrder->GetOrderId(),
                    bestAsk,
                    matchQuantity);

                // Resting order has been completely filled.
                if (askOrder->IsFilled())
                {
                    const OrderId askOrderId = askOrder->GetOrderId();

                    auto* orderEntry = orderIndex_.Find(askOrderId);

                    if (!orderEntry || orderEntry->location != askNode)
                    {
                        throw std::logic_error(
                            "Order book is inconsistent: "
                            "matched ask order not found or node mismatch."
                        );
                    }

                    // Remove the node from the price level.
                    askLevel.RemoveNode(askNode);

                    // Return the node to the reusable pool.
                    orderNodePool_.Release(askNode);

                    // Remove the order from the ID index.
                    orderIndex_.Erase(askOrderId);
                }
            }

            if (askLevel.Empty())
            {
                asks_.erase(askLevelIt);
            }
        }

        // =====================================================
        // SELL order matches against BID orders
        // =====================================================

        else
        {
            if (bids_.empty())
                break;

            auto bidLevelIt = bids_.begin();

            const Price bestBid = bidLevelIt->first;

            // Sell order cannot trade below its limit price.
            if (incomingOrder->GetOrderType() != OrderType::Market &&
                incomingOrder->GetPrice() > bestBid)
            {
                break;
            }

            PriceLevel& bidLevel = bidLevelIt->second;

            while (!bidLevel.Empty() && !incomingOrder->IsFilled())
            {
                // The head node is the oldest order at this price.
                PriceLevel::Node* bidNode = bidLevel.GetHead();

                const OrderPointer bidOrder = bidNode->order;

                const Quantity matchQuantity = std::min(incomingOrder->GetRemainingQuantity(), bidOrder->GetRemainingQuantity());

                incomingOrder->Fill(matchQuantity);
                bidOrder->Fill(matchQuantity);

                bidLevel.ReduceQuantity(matchQuantity);

                AddExecutionReports(
                    result.reports,
                    incomingOrder,
                    bestBid,
                    matchQuantity);

                AddExecutionReports(
                    result.reports,
                    bidOrder,
                    bestBid,
                    matchQuantity);

                result.trades.push_back(
                    Trade{
                        TradeInfo{
                            bidOrder->GetOrderId(),
                            bestBid,
                            matchQuantity
                        },
                        TradeInfo{
                            incomingOrder->GetOrderId(),
                            bestBid,
                            matchQuantity
                        }
                    });

                RecordEvent(
                    EventType::TradeExecuted,
                    incomingOrder->GetOrderId(),
                    bestBid,
                    matchQuantity);

                // Resting order has been completely filled.
                if (bidOrder->IsFilled())
                {
                    const OrderId bidOrderId = bidOrder->GetOrderId();

                    auto* orderEntry = orderIndex_.Find(bidOrderId);

                    if (!orderEntry || orderEntry->location != bidNode)
                    {
                        throw std::logic_error(
                            "Order book is inconsistent: "
                            "matched bid order not found or node mismatch."
                        );
                    }

                    // Remove the node from the price level.
                    bidLevel.RemoveNode(bidNode);

                    // Return the node to the reusable pool.
                    orderNodePool_.Release(bidNode);

                    // Remove the order from the ID index.
                    orderIndex_.Erase(bidOrderId);
                }
            }

            if (bidLevel.Empty())
            {
                bids_.erase(bidLevelIt);
            }
        }
    }

    return result;
}

Quantity OrderBook::GetBestAskQuantity() const
{
    if (asks_.empty())
    {
        throw std::logic_error(
            "No asks available.");
    }

    return asks_.begin()->second.GetTotalQuantity();
}

Quantity OrderBook::GetBestBidQuantity() const
{
    if (bids_.empty())
    {
        throw std::logic_error(
            "No bids available.");
    }

    return bids_.begin()->second.GetTotalQuantity();
}

OrderResult OrderBook::ModifyOrder(const OrderModify& modify)
{
    OrderResult result;

    auto* orderEntry = orderIndex_.Find(modify.orderId);

    if (!orderEntry)
    {
        return result;
    }

    OrderPointer order = orderEntry->order;

    if (!order)
    {
        throw std::logic_error(
            "Order book is inconsistent: order pointer is null.");
    }

    const Price oldPrice = order->GetPrice();
    const Quantity oldQuantity =
        order->GetRemainingQuantity();

    // ---------------------------------------------------------
    // Quantity = 0 means cancellation
    // ---------------------------------------------------------

    if (modify.quantity == 0)
    {
        CancelOrder(modify.orderId);
        return result;
    }

    // ---------------------------------------------------------
    // Case 1:
    // Same price + reduced quantity
    //
    // The order keeps its position in the FIFO queue.
    // ---------------------------------------------------------

    const bool priceChanged =
        modify.price != oldPrice;

    const bool quantityIncreased =
        modify.quantity > oldQuantity;

    if (!priceChanged && !quantityIncreased)
    {
        const Quantity reduction =
            oldQuantity - modify.quantity;

        if (reduction > 0)
        {
            order->Fill(reduction);

            PriceLevel& level =
                order->GetSide() == Side::Buy
                    ? bids_.at(oldPrice)
                    : asks_.at(oldPrice);

            level.ReduceQuantity(reduction);
        }

        RecordEvent(
            EventType::OrderModified,
            order->GetOrderId(),
            modify.price,
            modify.quantity);

        return result;
    }

    // ---------------------------------------------------------
    // Case 2:
    // Price changed OR quantity increased.
    //
    // The order loses its previous FIFO position.
    // ---------------------------------------------------------

    // const Side side = order->GetSide();

    RemoveOrderFromBook(modify.orderId);

    // Update the SAME Order object.
    order->Modify(
        modify.price,
        modify.quantity);

    // ---------------------------------------------------------
    // Try to match the modified order.
    // ---------------------------------------------------------

    result = MatchOrders(order);

    // ---------------------------------------------------------
    // If the order still has quantity remaining,
    // put it back into the book.
    // ---------------------------------------------------------

    if (!order->IsFilled())
    {
        if (order->GetOrderType() != OrderType::FillAndKill &&
            order->GetOrderType() != OrderType::Market)
        {
            AddOrderToBook(order);
        }
    }

    RecordEvent(
        EventType::OrderModified,
        order->GetOrderId(),
        order->GetPrice(),
        order->GetRemainingQuantity());

    return result;
}

void OrderBook::CancelGoodForDayOrders()
{
    std::vector<OrderId> ordersToCancel;

    orderIndex_.ForEach(
        [&ordersToCancel](
            OrderId orderId,
            const OrderPointer& order,
            OrderNodePool::Node*)
        {
            if (order->GetOrderType() == OrderType::GoodForDay)
            {
                ordersToCancel.push_back(orderId);
            }
        });

    for (const OrderId orderId : ordersToCancel)
    {
        CancelOrder(orderId);
    }
}

void OrderBook::AddExecutionReports(std::vector<ExecutionReport>& reports, const OrderPointer& order, Price executionPrice, Quantity executedQuantity) const
{
    ExecutionStatus status = order->IsFilled() ? ExecutionStatus::Filled : ExecutionStatus::PartiallyFilled;

    reports.push_back(
        ExecutionReport{
            order->GetOrderId(),
            executionPrice,
            executedQuantity,
            order->GetRemainingQuantity(),
            status
        });
}

void OrderBook::RecordEvent(
    EventType type,
    OrderId orderId,
    Price price,
    Quantity quantity)
{
    if (!eventLoggingEnabled_)
    {
        return;
    }

    eventLog_.push_back(
        EngineEvent{
            nextSequenceNumber_++,
            type,
            orderId,
            price,
            quantity
        });
}

const std::vector<EngineEvent>& OrderBook::GetEventLog() const noexcept
{
    return eventLog_;
}

bool OrderBook::ValidateBook() const
{
    // =========================================================
    // Check every bid price level
    // =========================================================

    for (const auto& [price, level] : bids_)
    {
        // Every existing price level must contain orders.
        if (level.Empty())
        {
            return false;
        }

        Quantity calculatedQuantity = 0;

        // Walk through the linked list of pool nodes.
        for (const PriceLevel::Node* node = level.GetHead(); node != nullptr; node = node->next)
        {
            const auto& order = node->order;

            if (!order)
            {
                return false;
            }

            if (order->GetSide() != Side::Buy)
            {
                return false;
            }

            if (order->GetPrice() != price)
            {
                return false;
            }

            if (order->IsFilled())
            {
                return false;
            }

            calculatedQuantity += order->GetRemainingQuantity();

            // The order must exist in the order index.
            const auto* orderEntry = orderIndex_.Find(order->GetOrderId());

            if (!orderEntry)
            {
                return false;
            }

            if (orderEntry->order != order)
            {
                return false;
            }

            if (orderEntry->location != node)
            {
                return false;
            }

            // The index must point back to this exact node.
            if (orderEntry->location != node)
            {
                return false;
            }
        }

        // Aggregated quantity must match the actual orders.
        if (calculatedQuantity != level.GetTotalQuantity())
        {
            return false;
        }
    }

    // =========================================================
    // Check every ask price level
    // =========================================================

    for (const auto& [price, level] : asks_)
    {
        if (level.Empty())
        {
            return false;
        }

        Quantity calculatedQuantity = 0;

        // Walk through the linked list of pool nodes.
        for (const PriceLevel::Node* node = level.GetHead(); node != nullptr; node = node->next)
        {
            const auto& order = node->order;

            if (!order)
            {
                return false;
            }

            if (order->GetSide() != Side::Sell)
            {
                return false;
            }

            if (order->GetPrice() != price)
            {
                return false;
            }

            if (order->IsFilled())
            {
                return false;
            }

            calculatedQuantity += order->GetRemainingQuantity();

            const auto* orderEntry = orderIndex_.Find(order->GetOrderId());

            if (!orderEntry)
            {
                return false;
            }

            if (orderEntry->order != order)
            {
                return false;
            }

            if (orderEntry->location != node)
            {
                return false;
            }

            // The index must point back to this exact node.
            if (orderEntry->location != node)
            {
                return false;
            }
        }

        if (calculatedQuantity != level.GetTotalQuantity())
        {
            return false;
        }
    }

    // =========================================================
    // Check that every indexed order actually exists
    // in the corresponding price level.
    // =========================================================

    bool indexValid = true;

    orderIndex_.ForEach(
        [&indexValid](
            OrderId orderId,
            const OrderPointer& order,
            OrderNodePool::Node* location)
        {
            if (!order ||
                order->GetOrderId() != orderId ||
                order->IsFilled() ||
                !location ||
                location->order != order)
            {
                indexValid = false;
            }
        });

    if (!indexValid)
    {
        return false;
    }

    // =========================================================
    // Best bid must always be below best ask.
    // =========================================================

    if (!bids_.empty() && !asks_.empty())
    {
        const Price bestBid =
            bids_.begin()->first;

        const Price bestAsk =
            asks_.begin()->first;

        if (bestBid >= bestAsk)
        {
            return false;
        }
    }

    return true;
}

void OrderBook::ReserveEventLog(std::size_t capacity)
{
    eventLog_.reserve(capacity);
}