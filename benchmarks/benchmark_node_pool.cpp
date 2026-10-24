#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

#include "Order.h"
#include "OrderNodePool.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"


int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    constexpr int ROUNDS = 10'000;

    OrderNodePool pool(ORDER_COUNT);

    std::vector<std::shared_ptr<Order>> orders;
    orders.reserve(ORDER_COUNT);

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        orders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Sell,
                10000,
                100));
    }

    for (int round = 0; round < ROUNDS; ++round)
    {
        std::vector<OrderNodePool::Node*> acquired;
        acquired.reserve(ORDER_COUNT);

        for (const auto& order : orders)
        {
            acquired.push_back(pool.Acquire(order));
        }

        for (auto* node : acquired)
        {
            pool.Release(node);
        }
    }

    std::cout << "Order node pool workload completed.\n";

    return 0;
}