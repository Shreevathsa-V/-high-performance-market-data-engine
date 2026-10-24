#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <functional>

#include "Order.h"
#include "OrderBook.h"
#include "OrderType.h"
#include "Side.h"

namespace
{
    using Clock = std::chrono::steady_clock;
    using OrderPointer = std::shared_ptr<Order>;

    constexpr std::size_t ORDER_COUNT = 50'000;
    constexpr std::size_t WARMUP_TRIALS = 5;
    constexpr std::size_t MEASURED_TRIALS = 20;

    constexpr Price BASE_PRICE = 10'000;
    constexpr Quantity ORDER_QUANTITY = 1;

    struct BenchmarkResult
    {
        double elapsedSeconds{};
    };

    OrderPointer MakeOrder(
        OrderId id,
        Side side,
        Price price,
        Quantity quantity = ORDER_QUANTITY)
    {
        return std::make_shared<Order>(
            OrderType::GoodTillCancel,
            id,
            side,
            price,
            quantity);
    }

    void Validate(
        const OrderBook& book,
        std::size_t expectedSize,
        const std::string& workload)
    {
        if (book.Size() != expectedSize ||
            !book.ValidateBook())
        {
            throw std::runtime_error(
                workload + ": order book validation failed.");
        }
    }

    // Each workload returns elapsed seconds for its timed section.
    // Order construction and book setup remain outside the timed section.

    double RunInsertSamePrice()
    {
        OrderBook book(false);

        std::vector<OrderPointer> orders;
        orders.reserve(ORDER_COUNT);

        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            orders.push_back(
                MakeOrder(
                    static_cast<OrderId>(i + 1),
                    Side::Buy,
                    BASE_PRICE));
        }

        const auto start = Clock::now();

        for (const auto& order : orders)
        {
            book.AddOrder(order);
        }

        const auto finish = Clock::now();

        Validate(book, ORDER_COUNT, "Same-price insertion");

        return std::chrono::duration<double>(
            finish - start).count();
    }

    double RunInsertDistinctPrices()
    {
        OrderBook book(false);

        std::vector<OrderPointer> orders;
        orders.reserve(ORDER_COUNT);

        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            orders.push_back(
                MakeOrder(
                    static_cast<OrderId>(i + 1),
                    Side::Buy,
                    BASE_PRICE + static_cast<Price>(i)));
        }

        const auto start = Clock::now();

        for (const auto& order : orders)
        {
            book.AddOrder(order);
        }

        const auto finish = Clock::now();

        Validate(book, ORDER_COUNT, "Distinct-price insertion");

        return std::chrono::duration<double>(
            finish - start).count();
    }

    double RunCancellation()
    {
        OrderBook book(false);

        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            book.AddOrder(
                MakeOrder(
                    static_cast<OrderId>(i + 1),
                    Side::Buy,
                    BASE_PRICE));
        }

        Validate(book, ORDER_COUNT, "Cancellation setup");

        const auto start = Clock::now();

        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            if (!book.CancelOrder(
                    static_cast<OrderId>(i + 1)))
            {
                throw std::runtime_error(
                    "Cancellation: expected order was not found.");
            }
        }

        const auto finish = Clock::now();

        if (!book.Empty() || !book.ValidateBook())
        {
            throw std::runtime_error(
                "Cancellation: final book validation failed.");
        }

        return std::chrono::duration<double>(
            finish - start).count();
    }

    double RunMatching()
    {
        OrderBook book(false);

        std::vector<OrderPointer> restingOrders;
        std::vector<OrderPointer> incomingOrders;

        restingOrders.reserve(ORDER_COUNT);
        incomingOrders.reserve(ORDER_COUNT);

        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            restingOrders.push_back(
                MakeOrder(
                    static_cast<OrderId>(i + 1),
                    Side::Sell,
                    BASE_PRICE));
        }

        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            incomingOrders.push_back(
                MakeOrder(
                    static_cast<OrderId>(ORDER_COUNT + i + 1),
                    Side::Buy,
                    BASE_PRICE));
        }

        // Build the resting ask queue before timing.
        for (const auto& order : restingOrders)
        {
            book.AddOrder(order);
        }

        Validate(book, ORDER_COUNT, "Matching setup");

        const auto start = Clock::now();

        for (const auto& order : incomingOrders)
        {
            book.AddOrder(order);
        }

        const auto finish = Clock::now();

        if (!book.Empty() || !book.ValidateBook())
        {
            throw std::runtime_error(
                "Matching: final book validation failed.");
        }

        return std::chrono::duration<double>(
            finish - start).count();
    }

    double Median(std::vector<double> values)
    {
        std::sort(values.begin(), values.end());

        const std::size_t n = values.size();

        if (n % 2 == 0)
        {
            return (values[n / 2 - 1] + values[n / 2]) / 2.0;
        }

        return values[n / 2];
    }

    void RunBenchmark(
        const std::string& name,
        const std::function<double()>& workload)
    {
        std::cout << name << '\n';

        // Warm-up: execute the workload but don't record its timing.
        for (std::size_t i = 0; i < WARMUP_TRIALS; ++i)
        {
            workload();
        }

        std::vector<double> elapsedTimes;
        elapsedTimes.reserve(MEASURED_TRIALS);

        for (std::size_t i = 0; i < MEASURED_TRIALS; ++i)
        {
            elapsedTimes.push_back(workload());
        }

        const double medianSeconds = Median(elapsedTimes);

        const double meanSeconds =
            std::accumulate(
                elapsedTimes.begin(),
                elapsedTimes.end(),
                0.0) / elapsedTimes.size();

        const auto [minIt, maxIt] = std::minmax_element(
            elapsedTimes.begin(),
            elapsedTimes.end());

        double squaredDifferences = 0.0;

        for (const double elapsed : elapsedTimes)
        {
            const double difference = elapsed - meanSeconds;
            squaredDifferences += difference * difference;
        }

        const double standardDeviation = std::sqrt(
            squaredDifferences / elapsedTimes.size());

        const double medianNsPerOperation =
            medianSeconds * 1'000'000'000.0 / ORDER_COUNT;

        const double medianThroughput =
            ORDER_COUNT / medianSeconds;

        std::cout
            << "  Measured trials:       " << MEASURED_TRIALS << '\n'
            << std::fixed
            << std::setprecision(2)
            << "  Median time/op:        "
            << medianNsPerOperation << " ns\n"
            << "  Mean trial time:       "
            << meanSeconds * 1'000.0 << " ms\n"
            << "  Min trial time:        "
            << *minIt * 1'000.0 << " ms\n"
            << "  Max trial time:        "
            << *maxIt * 1'000.0 << " ms\n"
            << "  Std deviation:         "
            << standardDeviation * 1'000.0 << " ms\n"
            << std::setprecision(0)
            << "  Median throughput:     "
            << medianThroughput << " operations/sec\n\n";
    }
}

int main()
{
    try
    {
        std::cout
            << "ORDER BOOK MICROBENCHMARK\n"
            << "=========================\n"
            << "Operations per trial: " << ORDER_COUNT << '\n'
            << "Warm-up trials:       " << WARMUP_TRIALS << '\n'
            << "Measured trials:      " << MEASURED_TRIALS << '\n'
            << "Event logging:        disabled\n"
            << "Order construction:   excluded from timed sections\n\n";

        RunBenchmark(
            "1. Insert orders at one price",
            RunInsertSamePrice);

        RunBenchmark(
            "2. Insert orders at distinct prices",
            RunInsertDistinctPrices);

        RunBenchmark(
            "3. Cancel existing orders",
            RunCancellation);

        RunBenchmark(
            "4. Match incoming orders",
            RunMatching);

        std::cout
            << "All workloads passed correctness validation.\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "BENCHMARK FAILED: "
            << exception.what() << '\n';

        return 1;
    }

    return 0;
}