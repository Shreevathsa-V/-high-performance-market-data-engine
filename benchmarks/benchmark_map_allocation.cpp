#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "Order.h"
#include "OrderNodePool.h"
#include "Usings.h"

struct Entry
{
    std::shared_ptr<Order> order;
    OrderNodePool::Node* location;
};

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    constexpr std::size_t ROUNDS = 10'000;

    std::unordered_map<OrderId, Entry> orders;
    orders.reserve(ORDER_COUNT);

    std::vector<std::shared_ptr<Order>> orderObjects;
    orderObjects.reserve(ORDER_COUNT);

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        orderObjects.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Sell,
                10000,
                100));
    }

    for (std::size_t round = 0; round < ROUNDS; ++round)
    {
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            orders.emplace(
                static_cast<OrderId>(i + 1),
                Entry{
                    orderObjects[i],
                    nullptr
                });
        }

        orders.clear();
    }

    std::cout << "Completed " << ROUNDS
              << " rounds.\n";

    std::cout << "Final map size: "
              << orders.size()
              << '\n';

    return 0;
}