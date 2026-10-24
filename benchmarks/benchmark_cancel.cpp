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

    OrderBook orderBook(false);

    std::vector<std::shared_ptr<Order>> orders;
    orders.reserve(ORDER_COUNT);

    // ---------------------------------------------------------
    // Create orders outside the timed region.
    // ---------------------------------------------------------

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        orders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Buy,
                10000,
                100));
    }

    // ---------------------------------------------------------
    // Add all orders BEFORE the timed cancellation benchmark.
    // ---------------------------------------------------------

    for (const auto& order : orders)
    {
        orderBook.AddOrder(order);
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
    // Benchmark ONLY CancelOrder().
    // ---------------------------------------------------------

    const auto start = Clock::now();

    for (const auto& order : orders)
    {
        orderBook.CancelOrder(order->GetOrderId());
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

    const double nsPerCancel =
        static_cast<double>(elapsed) /
        static_cast<double>(ORDER_COUNT);

    const double cancelsPerSecond =
        static_cast<double>(ORDER_COUNT) /
        (static_cast<double>(elapsed) /
         1'000'000'000.0);

    // ---------------------------------------------------------
    // Results
    // ---------------------------------------------------------

    std::cout
        << "\nCancellation operations: "
        << ORDER_COUNT
        << '\n';

    std::cout
        << "Total cancellation time: "
        << elapsed
        << " ns\n";

    std::cout
        << "Average CancelOrder: "
        << nsPerCancel
        << " ns/cancel\n";

    std::cout
        << "Cancellation throughput: "
        << cancelsPerSecond
        << " cancels/sec\n";

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