#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "Order.h"
#include "OrderBook.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

constexpr std::size_t ORDER_COUNT = 100'000;
constexpr int ROUNDS = 1000;

int main()
{
    std::cout << "Starting profiling workload...\n";

    for (int round = 0; round < ROUNDS; ++round)
    {
        OrderBook orderBook(false);

        std::vector<std::shared_ptr<Order>> restingOrders;
        std::vector<std::shared_ptr<Order>> incomingOrders;

        restingOrders.reserve(ORDER_COUNT);
        incomingOrders.reserve(ORDER_COUNT);

        // --------------------------------------------------------
        // Create resting SELL orders
        // --------------------------------------------------------

        for (OrderId i = 1; i <= ORDER_COUNT; ++i)
        {
            restingOrders.push_back(
                std::make_shared<Order>(
                    OrderType::GoodTillCancel,
                    i,
                    Side::Sell,
                    10'000,
                    100));
        }

        // --------------------------------------------------------
        // Add resting orders
        // --------------------------------------------------------

        for (const auto& order : restingOrders)
        {
            orderBook.AddOrder(order);
        }

        // --------------------------------------------------------
        // Create incoming BUY orders
        // --------------------------------------------------------

        for (OrderId i = 0; i < ORDER_COUNT; ++i)
        {
            incomingOrders.push_back(
                std::make_shared<Order>(
                    OrderType::GoodTillCancel,
                    ORDER_COUNT + i + 1,
                    Side::Buy,
                    10'100,
                    100));
        }

        // --------------------------------------------------------
        // Matching workload
        // --------------------------------------------------------

        for (const auto& order : incomingOrders)
        {
            orderBook.AddOrder(order);
        }

        if (!orderBook.Empty())
        {
            std::cerr << "ERROR: Book should be empty\n";
            return 1;
        }

        if (!orderBook.ValidateBook())
        {
            std::cerr << "ERROR: Book validation failed\n";
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

    std::cout << "Profiling workload complete.\n";

    return 0;
}