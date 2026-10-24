#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "Order.h"
#include "OrderIndex.h"
#include "OrderNodePool.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    constexpr std::size_t ROUNDS = 10'000;
    constexpr std::size_t BUCKET_COUNT = 262'144;

    OrderNodePool nodePool(ORDER_COUNT);

    OrderIndex index(
        ORDER_COUNT,
        BUCKET_COUNT);

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

    std::vector<OrderNodePool::Node*> nodes;
    nodes.reserve(ORDER_COUNT);

    for (std::size_t round = 0; round < ROUNDS; ++round)
    {
        nodes.clear();

        // Insert all orders.
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            auto* node =
                nodePool.Acquire(orders[i]);

            nodes.push_back(node);

            index.Insert(
                orders[i]->GetOrderId(),
                orders[i],
                node);
        }

        // Find every order.
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            auto* entry =
                index.Find(
                    orders[i]->GetOrderId());

            if (!entry)
            {
                std::cerr
                    << "ERROR: Order not found.\n";

                return 1;
            }

            if (entry->order != orders[i])
            {
                std::cerr
                    << "ERROR: Wrong order returned.\n";

                return 1;
            }
        }

        // Erase all orders.
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            index.Erase(
                orders[i]->GetOrderId());

            nodePool.Release(nodes[i]);
        }
    }

    std::cout
        << "Completed "
        << ROUNDS
        << " rounds.\n";

    std::cout
        << "Final index size: "
        << index.Size()
        << '\n';

    std::cout
        << "Final node pool reused successfully.\n";

    return 0;
}