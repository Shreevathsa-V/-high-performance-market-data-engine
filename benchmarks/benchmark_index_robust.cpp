#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Order.h"
#include "OrderIndex.h"
#include "OrderNodePool.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

using Clock = std::chrono::steady_clock;

constexpr std::size_t ORDER_COUNT = 50'000;
constexpr std::size_t BUCKET_COUNT = 262'144;
constexpr int WARMUP_TRIALS = 3;
constexpr int MEASURED_TRIALS = 15;

struct MapEntry
{
    std::shared_ptr<Order> order;
    OrderNodePool::Node* location;
};

struct Timing
{
    double insertNs{};
    double findNs{};
    double eraseNs{};
};

using Nanoseconds = std::chrono::nanoseconds;

template <typename Function>
double MeasureNanoseconds(Function&& function)
{
    const auto start = Clock::now();
    function();
    const auto finish = Clock::now();

    return static_cast<double>(
        std::chrono::duration_cast<Nanoseconds>(
            finish - start).count());
}

std::vector<OrderId> MakeIds(const std::string& workload)
{
    std::vector<OrderId> ids;
    ids.reserve(ORDER_COUNT);

    if (workload == "sequential")
    {
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            ids.push_back(static_cast<OrderId>(i + 1));
        }
    }
    else if (workload == "shuffled")
    {
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            ids.push_back(static_cast<OrderId>(i + 1));
        }

        std::mt19937_64 generator(123456789);
        std::shuffle(ids.begin(), ids.end(), generator);
    }
    else if (workload == "collisions")
    {
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            ids.push_back(
                static_cast<OrderId>(
                    1 + i * BUCKET_COUNT));
        }
    }
    else
    {
        throw std::invalid_argument("Unknown workload");
    }

    return ids;
}

std::vector<std::shared_ptr<Order>> MakeOrders(
    const std::vector<OrderId>& ids)
{
    std::vector<std::shared_ptr<Order>> orders;
    orders.reserve(ids.size());

    for (OrderId id : ids)
    {
        orders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                id,
                Side::Sell,
                10'000,
                100));
    }

    return orders;
}

Timing RunCustom(
    const std::vector<std::shared_ptr<Order>>& orders)
{
    OrderIndex index(ORDER_COUNT, BUCKET_COUNT);
    OrderNodePool pool(ORDER_COUNT);

    std::vector<OrderNodePool::Node*> nodes;
    nodes.reserve(ORDER_COUNT);

    std::uint64_t checksum = 0;

    Timing timing;

    timing.insertNs = MeasureNanoseconds([&]
    {
        for (const auto& order : orders)
        {
            auto* node = pool.Acquire(order);
            nodes.push_back(node);

            index.Insert(
                order->GetOrderId(),
                order,
                node);
        }
    });

    if (index.Size() != ORDER_COUNT)
    {
        throw std::runtime_error(
            "Custom index insertion validation failed");
    }

    timing.findNs = MeasureNanoseconds([&]
    {
        for (const auto& order : orders)
        {
            auto* entry = index.Find(order->GetOrderId());

            if (!entry || entry->order != order)
            {
                throw std::runtime_error(
                    "Custom index lookup validation failed");
            }

            checksum += entry->order->GetOrderId();
        }
    });

    timing.eraseNs = MeasureNanoseconds([&]
    {
        for (std::size_t i = 0; i < orders.size(); ++i)
        {
            index.Erase(orders[i]->GetOrderId());
            pool.Release(nodes[i]);
        }
    });

    if (index.Size() != 0 || checksum == 0)
    {
        throw std::runtime_error(
            "Custom index final validation failed");
    }

    return timing;
}

Timing RunStandard(
    const std::vector<std::shared_ptr<Order>>& orders)
{
    std::unordered_map<OrderId, MapEntry> index;
    index.reserve(ORDER_COUNT);

    OrderNodePool pool(ORDER_COUNT);

    std::vector<OrderNodePool::Node*> nodes;
    nodes.reserve(ORDER_COUNT);

    std::uint64_t checksum = 0;

    Timing timing;

    timing.insertNs = MeasureNanoseconds([&]
    {
        for (const auto& order : orders)
        {
            auto* node = pool.Acquire(order);
            nodes.push_back(node);

            const auto [it, inserted] = index.emplace(
                order->GetOrderId(),
                MapEntry{order, node});

            if (!inserted)
            {
                throw std::runtime_error(
                    "Duplicate ID in standard map");
            }

            (void)it;
        }
    });

    if (index.size() != ORDER_COUNT)
    {
        throw std::runtime_error(
            "Standard map insertion validation failed");
    }

    timing.findNs = MeasureNanoseconds([&]
    {
        for (const auto& order : orders)
        {
            auto it = index.find(order->GetOrderId());

            if (it == index.end() || it->second.order != order)
            {
                throw std::runtime_error(
                    "Standard map lookup validation failed");
            }

            checksum += it->second.order->GetOrderId();
        }
    });

    timing.eraseNs = MeasureNanoseconds([&]
    {
        for (std::size_t i = 0; i < orders.size(); ++i)
        {
            auto it = index.find(orders[i]->GetOrderId());

            if (it == index.end())
            {
                throw std::runtime_error(
                    "Standard map erase validation failed");
            }

            index.erase(it);
            pool.Release(nodes[i]);
        }
    });

    if (!index.empty() || checksum == 0)
    {
        throw std::runtime_error(
            "Standard map final validation failed");
    }

    return timing;
}

double Median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());

    const std::size_t middle = values.size() / 2;

    if (values.size() % 2 != 0)
    {
        return values[middle];
    }

    return (values[middle - 1] + values[middle]) / 2.0;
}

void RunWorkload(const std::string& workload)
{
    const auto ids = MakeIds(workload);
    const auto orders = MakeOrders(ids);

    std::vector<double> customInsert;
    std::vector<double> customFind;
    std::vector<double> customErase;

    std::vector<double> standardInsert;
    std::vector<double> standardFind;
    std::vector<double> standardErase;

    // Alternate execution order to reduce systematic ordering bias.
    for (int trial = 0;
         trial < WARMUP_TRIALS + MEASURED_TRIALS;
         ++trial)
    {
        Timing custom;
        Timing standard;

        if (trial % 2 == 0)
        {
            custom = RunCustom(orders);
            standard = RunStandard(orders);
        }
        else
        {
            standard = RunStandard(orders);
            custom = RunCustom(orders);
        }

        if (trial >= WARMUP_TRIALS)
        {
            customInsert.push_back(
                custom.insertNs / ORDER_COUNT);

            customFind.push_back(
                custom.findNs / ORDER_COUNT);

            customErase.push_back(
                custom.eraseNs / ORDER_COUNT);

            standardInsert.push_back(
                standard.insertNs / ORDER_COUNT);

            standardFind.push_back(
                standard.findNs / ORDER_COUNT);

            standardErase.push_back(
                standard.eraseNs / ORDER_COUNT);
        }
    }

    std::cout << "\n========== " << workload
              << " IDs ==========\n";

    std::cout << std::fixed << std::setprecision(2);

    std::cout
        << "Operation       unordered_map   Custom Index\n"
        << "Insert (ns/op)  "
        << std::setw(13) << Median(standardInsert)
        << "   "
        << std::setw(12) << Median(customInsert)
        << '\n';

    std::cout
        << "Find (ns/op)    "
        << std::setw(13) << Median(standardFind)
        << "   "
        << std::setw(12) << Median(customFind)
        << '\n';

    std::cout
        << "Erase (ns/op)   "
        << std::setw(13) << Median(standardErase)
        << "   "
        << std::setw(12) << Median(customErase)
        << '\n';
}

int main()
{
    std::cout
        << "ORDER INDEX COMPARISON\n"
        << "Orders per trial: " << ORDER_COUNT << '\n'
        << "Warm-up trials: " << WARMUP_TRIALS << '\n'
        << "Measured trials: " << MEASURED_TRIALS << '\n'
        << "Bucket count: " << BUCKET_COUNT << '\n';

    try
    {
        RunWorkload("sequential");
        RunWorkload("shuffled");
        RunWorkload("collisions");
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Benchmark failed: "
            << error.what() << '\n';

        return 1;
    }

    std::cout << "\nAll benchmark workloads passed validation.\n";

    return 0;
}