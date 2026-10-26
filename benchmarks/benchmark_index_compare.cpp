#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Order.h"
#include "OrderIndex.h"
#include "OrderNodePool.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

using Clock = std::chrono::steady_clock;

struct MapEntry
{
    std::shared_ptr<Order> order;
    OrderNodePool::Node* location;
};

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    //constexpr std::size_t ROUNDS = 100;

    constexpr std::size_t BUCKET_COUNT = 262'144;

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

    // =========================================================
    // std::unordered_map
    // =========================================================

    std::unordered_map<OrderId, MapEntry> standardMap;
    standardMap.reserve(ORDER_COUNT);

    OrderNodePool standardMapPool(ORDER_COUNT);

    std::vector<OrderNodePool::Node*> standardMapNodes;
    standardMapNodes.reserve(ORDER_COUNT);

    std::uint64_t standardChecksum = 0;

    // Warm-up
    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto* node =
            standardMapPool.Acquire(orders[i]);

        standardMapNodes.push_back(node);

        standardMap.emplace(
            orders[i]->GetOrderId(),
            MapEntry{
                orders[i],
                node
            });
    }

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto it =
            standardMap.find(
                orders[i]->GetOrderId());

        if (it != standardMap.end())
        {
            standardChecksum +=
                it->second.order->GetOrderId();
        }
    }

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        standardMap.erase(
            orders[i]->GetOrderId());

        standardMapPool.Release(
            standardMapNodes[i]);
    }

    standardMapNodes.clear();

    // =========================================================
    // Timed std::unordered_map
    // =========================================================

    std::uint64_t mapInsertChecksum = 0;
    std::uint64_t mapFindChecksum = 0;
    std::uint64_t mapEraseChecksum = 0;

    const auto mapInsertStart = Clock::now();

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto* node =
            standardMapPool.Acquire(orders[i]);

        standardMapNodes.push_back(node);

        standardMap.emplace(
            orders[i]->GetOrderId(),
            MapEntry{
                orders[i],
                node
            });

        mapInsertChecksum +=
            orders[i]->GetOrderId();
    }

    const auto mapInsertEnd = Clock::now();

    const auto mapFindStart = Clock::now();

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto it =
            standardMap.find(
                orders[i]->GetOrderId());

        if (it != standardMap.end())
        {
            mapFindChecksum +=
                it->second.order->GetOrderId();
        }
    }

    const auto mapFindEnd = Clock::now();

    const auto mapEraseStart = Clock::now();

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto it =
            standardMap.find(
                orders[i]->GetOrderId());

        if (it != standardMap.end())
        {
            mapEraseChecksum +=
                it->second.order->GetOrderId();

            standardMap.erase(it);
        }

        standardMapPool.Release(
            standardMapNodes[i]);
    }

    const auto mapEraseEnd = Clock::now();

    // =========================================================
    // Custom OrderIndex
    // =========================================================

    OrderIndex customIndex(
        ORDER_COUNT,
        BUCKET_COUNT);

    OrderNodePool customIndexPool(ORDER_COUNT);

    std::vector<OrderNodePool::Node*> customNodes;
    customNodes.reserve(ORDER_COUNT);

    std::uint64_t customInsertChecksum = 0;
    std::uint64_t customFindChecksum = 0;
    std::uint64_t customEraseChecksum = 0;

    // Warm-up
    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto* node =
            customIndexPool.Acquire(orders[i]);

        customNodes.push_back(node);

        customIndex.Insert(
            orders[i]->GetOrderId(),
            orders[i],
            node);
    }

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto* entry =
            customIndex.Find(
                orders[i]->GetOrderId());

        if (entry)
        {
            customFindChecksum +=
                entry->order->GetOrderId();
        }
    }

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        customIndex.Erase(
            orders[i]->GetOrderId());

        customIndexPool.Release(
            customNodes[i]);
    }

    customNodes.clear();

    // =========================================================
    // Timed custom OrderIndex
    // =========================================================

    const auto customInsertStart = Clock::now();

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto* node =
            customIndexPool.Acquire(orders[i]);

        customNodes.push_back(node);

        customIndex.Insert(
            orders[i]->GetOrderId(),
            orders[i],
            node);

        customInsertChecksum +=
            orders[i]->GetOrderId();
    }

    const auto customInsertEnd = Clock::now();

    const auto customFindStart = Clock::now();

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto* entry =
            customIndex.Find(
                orders[i]->GetOrderId());

        if (entry)
        {
            customFindChecksum +=
                entry->order->GetOrderId();
        }
    }

    const auto customFindEnd = Clock::now();

    const auto customEraseStart = Clock::now();

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto* entry =
            customIndex.Find(
                orders[i]->GetOrderId());

        if (entry)
        {
            customEraseChecksum +=
                entry->order->GetOrderId();

            customIndex.Erase(
                orders[i]->GetOrderId());
        }

        customIndexPool.Release(
            customNodes[i]);
    }

    const auto customEraseEnd = Clock::now();

    // =========================================================
    // Calculate timings
    // =========================================================

    const double mapInsertNs =
        static_cast<double>(
            std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                mapInsertEnd - mapInsertStart)
                .count())
        / ORDER_COUNT;

    const double mapFindNs =
        static_cast<double>(
            std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                mapFindEnd - mapFindStart)
                .count())
        / ORDER_COUNT;

    const double mapEraseNs =
        static_cast<double>(
            std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                mapEraseEnd - mapEraseStart)
                .count())
        / ORDER_COUNT;

    const double customInsertNs =
        static_cast<double>(
            std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                customInsertEnd - customInsertStart)
                .count())
        / ORDER_COUNT;

    const double customFindNs =
        static_cast<double>(
            std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                customFindEnd - customFindStart)
                .count())
        / ORDER_COUNT;

    const double customEraseNs =
        static_cast<double>(
            std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                customEraseEnd - customEraseStart)
                .count())
        / ORDER_COUNT;

    std::cout << std::fixed
              << std::setprecision(3);

    std::cout
        << "\n=== std::unordered_map ===\n";

    std::cout
        << "Insert: "
        << mapInsertNs
        << " ns/op\n";

    std::cout
        << "Find:   "
        << mapFindNs
        << " ns/op\n";

    std::cout
        << "Erase:  "
        << mapEraseNs
        << " ns/op\n";

    std::cout
        << "\n=== Custom OrderIndex ===\n";

    std::cout
        << "Insert: "
        << customInsertNs
        << " ns/op\n";

    std::cout
        << "Find:   "
        << customFindNs
        << " ns/op\n";

    std::cout
        << "Erase:  "
        << customEraseNs
        << " ns/op\n";

    std::cout
        << "\nChecksums:\n";

    std::cout
        << "Map: "
        << standardChecksum
        << " / "
        << mapInsertChecksum
        << " / "
        << mapFindChecksum
        << " / "
        << mapEraseChecksum
        << '\n';

    std::cout
        << "Custom: "
        << customFindChecksum
        << " / "
        << customInsertChecksum
        << " / "
        << customFindChecksum
        << " / "
        << customEraseChecksum
        << '\n';

    std::cout
        << "\nFinal map size: "
        << standardMap.size()
        << '\n';

    std::cout
        << "Final custom index size: "
        << customIndex.Size()
        << '\n';

    return 0;
}