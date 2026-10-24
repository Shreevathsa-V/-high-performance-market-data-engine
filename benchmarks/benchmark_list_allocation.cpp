#include <chrono>
#include <cstddef>
#include <iostream>
#include <list>
#include <memory>
#include <vector>

#include "Order.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

using Clock = std::chrono::steady_clock;

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    constexpr int ROUNDS = 10'000;

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

    std::list<std::shared_ptr<Order>> orderList;

    for (int round = 0; round < ROUNDS; ++round)
    {
        for (const auto& order : orders)
        {
            orderList.push_back(order);
        }

        while (!orderList.empty())
        {
            orderList.pop_front();
        }
    }

    std::cout << "List allocation workload completed.\n";
    return 0;
}