#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "Order.h"
#include "OrderBook.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

using Clock = std::chrono::steady_clock;

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;

    OrderBook orderBook;

    std::vector<std::shared_ptr<Order>> incomingOrders;
    incomingOrders.reserve(ORDER_COUNT);


    // ---------------------------------------------------------
    // Create resting SELL orders.
    //
    // These are NOT part of the benchmark timer.
    // ---------------------------------------------------------

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto sellOrder = std::make_shared<Order>(
            OrderType::GoodTillCancel,
            static_cast<OrderId>(i + 1),
            Side::Sell,
            10000,
            100);

        orderBook.AddOrder(sellOrder);
    }


    // ---------------------------------------------------------
    // Pre-create incoming BUY orders.
    //
    // Allocation is deliberately outside the timed region.
    // ---------------------------------------------------------

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        incomingOrders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(
                    ORDER_COUNT + i + 1),
                Side::Buy,
                10100,
                100));
    }


    std::cout
        << "Initial book size: "
        << orderBook.Size()
        << '\n';

    std::cout
        << "Initial book valid: "
        << std::boolalpha
        << orderBook.ValidateBook()
        << '\n';


    // ---------------------------------------------------------
    // Benchmark ONLY the engine processing.
    // ---------------------------------------------------------

    const auto start = Clock::now();

    for (const auto& buyOrder : incomingOrders)
    {
        orderBook.AddOrder(buyOrder);
    }

    const auto end = Clock::now();


    // ---------------------------------------------------------
    // Calculate results.
    // ---------------------------------------------------------

    const auto elapsed =
        std::chrono::duration_cast<
            std::chrono::nanoseconds>(
                end - start)
            .count();

    const double nsPerMatch =
        static_cast<double>(elapsed) /
        static_cast<double>(ORDER_COUNT);

    const double matchesPerSecond =
        static_cast<double>(ORDER_COUNT) /
        (static_cast<double>(elapsed) /
         1'000'000'000.0);


    // ---------------------------------------------------------
    // Results
    // ---------------------------------------------------------

    std::cout
        << "\nMatching operations: "
        << ORDER_COUNT
        << '\n';

    std::cout
        << "Total matching time: "
        << elapsed
        << " ns\n";

    std::cout
        << "Average engine processing: "
        << nsPerMatch
        << " ns/order\n";

    std::cout
        << "Engine throughput: "
        << matchesPerSecond
        << " orders/sec\n";

    std::cout
        << "Final book size: "
        << orderBook.Size()
        << '\n';

    std::cout
        << "Final book valid: "
        << orderBook.ValidateBook()
        << '\n';

    return 0;
}