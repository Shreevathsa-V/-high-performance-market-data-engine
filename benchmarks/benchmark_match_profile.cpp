#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "Order.h"
#include "OrderBook.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    constexpr int ROUNDS = 100;

    std::vector<std::shared_ptr<Order>> restingOrders;
    std::vector<std::shared_ptr<Order>> incomingOrders;

    restingOrders.reserve(ORDER_COUNT);
    incomingOrders.reserve(ORDER_COUNT);

    // ---------------------------------------------------------
    // Create all Order objects outside the profiled workload.
    // ---------------------------------------------------------

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        restingOrders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Sell,
                10000,
                100));

        incomingOrders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(
                    ORDER_COUNT + i + 1),
                Side::Buy,
                10100,
                100));
    }

    // ---------------------------------------------------------
    // Repeated matching workload.
    // ---------------------------------------------------------

    for (int round = 0; round < ROUNDS; ++round)
    {
        OrderBook orderBook(false);

        orderBook.ReserveEventLog(ORDER_COUNT);

        // Build the resting book.
        for (const auto& order : restingOrders)
        {
            order->Modify(10000, 100);
            orderBook.AddOrder(order);
        }

        // Match every incoming order.
        for (const auto& order : incomingOrders)
        {
            order->Modify(10100, 100);
            orderBook.AddOrder(order);
        }

        if (!orderBook.Empty())
        {
            std::cerr << "ERROR: book is not empty\n";
            return 1;
        }

        if (!orderBook.ValidateBook())
        {
            std::cerr << "ERROR: book validation failed\n";
            return 1;
        }

        if ((round + 1) % 10 == 0)
        {
            std::cout
                << "Completed round "
                << (round + 1)
                << '\n';
        }
    }

    std::cout << "Profiling workload completed successfully.\n";

    return 0;
}