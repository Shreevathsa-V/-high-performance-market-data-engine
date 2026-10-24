#include <iostream>
#include <string>
#include <cassert>

#include "OrderBook.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

std::string EventTypeToString(EventType type)
{
    switch (type)
    {
        case EventType::OrderAdded:
            return "OrderAdded";

        case EventType::OrderCancelled:
            return "OrderCancelled";

        case EventType::OrderModified:
            return "OrderModified";

        case EventType::TradeExecuted:
            return "TradeExecuted";
    }

    return "Unknown";
}

void RunInvariantTest()
{
    OrderBook orderBook;

    auto sell1 = std::make_shared<Order>(
        OrderType::GoodTillCancel,
        1,
        Side::Sell,
        10000,
        100);

    auto sell2 = std::make_shared<Order>(
        OrderType::GoodTillCancel,
        2,
        Side::Sell,
        10100,
        200);

    auto buy1 = std::make_shared<Order>(
        OrderType::GoodTillCancel,
        3,
        Side::Buy,
        9900,
        150);

    auto buy2 = std::make_shared<Order>(
        OrderType::GoodTillCancel,
        4,
        Side::Buy,
        9800,
        100);


    // ---------------------------------------------------------
    // ADD SELL #1
    // ---------------------------------------------------------

    orderBook.AddOrder(sell1);

    assert(orderBook.ValidateBook());


    // ---------------------------------------------------------
    // ADD SELL #2
    // ---------------------------------------------------------

    orderBook.AddOrder(sell2);

    assert(orderBook.ValidateBook());


    // ---------------------------------------------------------
    // ADD BUY #1
    // ---------------------------------------------------------

    orderBook.AddOrder(buy1);

    assert(orderBook.ValidateBook());


    // ---------------------------------------------------------
    // ADD BUY #2
    // ---------------------------------------------------------

    orderBook.AddOrder(buy2);

    assert(orderBook.ValidateBook());


    // ---------------------------------------------------------
    // MODIFY BUY #2
    //
    // 9800 -> 10200
    //
    // This crosses the asks.
    // ---------------------------------------------------------

    OrderModify modify{
        4,
        10200,
        100
    };

    orderBook.ModifyOrder(modify);

    assert(orderBook.ValidateBook());


    // ---------------------------------------------------------
    // CANCEL SELL #2
    // ---------------------------------------------------------

    orderBook.CancelOrder(2);

    assert(orderBook.ValidateBook());


    std::cout
        << "Invariant test passed.\n";
}

int main()
{
    RunInvariantTest();

    std::cout << '\n';

    OrderBook orderBook;

    // ---------------------------------------------------------
    // 1. Add SELL order
    // ---------------------------------------------------------

    auto sellOrder = std::make_shared<Order>(
        OrderType::GoodTillCancel,
        1,
        Side::Sell,
        10000,
        100);

    orderBook.AddOrder(sellOrder);


    // ---------------------------------------------------------
    // 2. Add BUY order below the ask
    // ---------------------------------------------------------

    auto buyOrder = std::make_shared<Order>(
        OrderType::GoodTillCancel,
        2,
        Side::Buy,
        9900,
        100);

    orderBook.AddOrder(buyOrder);


    std::cout << "Before modification:\n";
    std::cout
        << "BUY #2 remaining: "
        << buyOrder->GetRemainingQuantity()
        << '\n';

    std::cout
        << "SELL #1 remaining: "
        << sellOrder->GetRemainingQuantity()
        << "\n\n";


    // ---------------------------------------------------------
    // 3. Modify BUY #2
    //
    // 9900 -> 10100
    //
    // This crosses the 10000 ask.
    // ---------------------------------------------------------

    OrderModify modify{
        2,
        10100,
        100
    };

    OrderResult result =
        orderBook.ModifyOrder(modify);


    // ---------------------------------------------------------
    // 4. Print trades
    // ---------------------------------------------------------

    std::cout
        << "Trades generated: "
        << result.trades.size()
        << "\n\n";

    for (const auto& trade : result.trades)
    {
        std::cout
            << "Bid Order: " << trade.bid.orderId
            << " | Ask Order: " << trade.ask.orderId
            << " | Price: " << trade.bid.price
            << " | Quantity: " << trade.bid.quantity
            << '\n';
    }


    // ---------------------------------------------------------
    // 5. Print remaining quantities
    // ---------------------------------------------------------

    std::cout << "\nRemaining quantities:\n";

    std::cout
        << "BUY #2: "
        << buyOrder->GetRemainingQuantity()
        << '\n';

    std::cout
        << "SELL #1: "
        << sellOrder->GetRemainingQuantity()
        << '\n';


    // ---------------------------------------------------------
    // 6. Print event log
    // ---------------------------------------------------------

    const auto& eventLog =
        orderBook.GetEventLog();

    std::cout
        << "\nEvent count: "
        << eventLog.size()
        << "\n\n";

    for (const auto& event : eventLog)
    {
        std::cout
            << "Seq: " << event.sequenceNumber
            << " | Type: "
            << EventTypeToString(event.type)
            << " | Order ID: "
            << event.orderId
            << " | Price: "
            << event.price
            << " | Quantity: "
            << event.quantity
            << '\n';
    }

    std::cout
        << "\nBook valid: "
        << std::boolalpha
        << orderBook.ValidateBook()
        << '\n';

    return 0;
}