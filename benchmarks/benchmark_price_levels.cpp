#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Order.h"
#include "OrderBook.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

struct BenchmarkResult
{
    std::string scenario;
    double averageNs{};
    double p50Ns{};
    double p90Ns{};
    double p99Ns{};
    double p999Ns{};
    double throughput{};
    std::size_t finalBookSize{};
    bool bookValid{};
};

double Percentile(
    const std::vector<std::uint64_t>& sortedSamples,
    double percentile)
{
    if (sortedSamples.empty())
    {
        return 0.0;
    }

    const double index =
        (percentile / 100.0) *
        static_cast<double>(sortedSamples.size() - 1);

    const auto lower = static_cast<std::size_t>(index);
    const auto upper = std::min(
        lower + 1,
        sortedSamples.size() - 1);

    const double fraction =
        index - static_cast<double>(lower);

    return static_cast<double>(sortedSamples[lower]) +
        fraction * static_cast<double>(
            sortedSamples[upper] - sortedSamples[lower]);
}

BenchmarkResult RunBenchmark(
    const std::string& scenario,
    std::size_t orderCount,
    std::size_t priceLevelCount,
    Price incomingBuyPrice,
    std::size_t expectedFinalBookSize)
{
    constexpr std::size_t WARMUP_COUNT = 5'000;
    constexpr Price BASE_PRICE = 10'000;
    constexpr Quantity ORDER_QUANTITY = 100;

    // Warm-up uses a separate order book.
    {
        OrderBook warmupBook(false);

        for (std::size_t i = 0; i < WARMUP_COUNT; ++i)
        {
            auto sellOrder = std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Sell,
                BASE_PRICE,
                ORDER_QUANTITY);

            warmupBook.AddOrder(sellOrder);
        }

        for (std::size_t i = 0; i < WARMUP_COUNT; ++i)
        {
            auto buyOrder = std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(WARMUP_COUNT + i + 1),
                Side::Buy,
                BASE_PRICE + 1,
                ORDER_QUANTITY);

            warmupBook.AddOrder(buyOrder);
        }

        if (!warmupBook.ValidateBook() || !warmupBook.Empty())
        {
            throw std::runtime_error(
                "Warm-up order-book validation failed.");
        }
    }

    // Disable event logging so the benchmark focuses on matching
    // and order-book operations rather than event-log allocation.
    constexpr std::size_t POOL_CAPACITY = 200'000;
    constexpr std::size_t INDEX_CAPACITY = 200'000;
    constexpr std::size_t INDEX_BUCKET_COUNT = 524'288;

    OrderBook book(
        false,
        POOL_CAPACITY,
        INDEX_CAPACITY,
        INDEX_BUCKET_COUNT);

    // Populate resting sell orders before measurement.
    for (std::size_t i = 0; i < orderCount; ++i)
    {
        const Price price = BASE_PRICE +
            static_cast<Price>(i % priceLevelCount);

        auto sellOrder = std::make_shared<Order>(
            OrderType::GoodTillCancel,
            static_cast<OrderId>(i + 1),
            Side::Sell,
            price,
            ORDER_QUANTITY);

        book.AddOrder(sellOrder);
    }

    if (!book.ValidateBook() || book.Size() != orderCount)
    {
        throw std::runtime_error(
            "Initial order-book validation failed.");
    }

    // Create incoming orders before the timed section.
    std::vector<std::shared_ptr<Order>> incomingOrders;
    incomingOrders.reserve(orderCount);

    for (std::size_t i = 0; i < orderCount; ++i)
    {
        incomingOrders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(orderCount + i + 1),
                Side::Buy,
                incomingBuyPrice,
                ORDER_QUANTITY));
    }

    std::vector<std::uint64_t> latencies;
    latencies.reserve(orderCount);

    const auto batchStart = Clock::now();

    for (const auto& order : incomingOrders)
    {
        const auto start = Clock::now();
        book.AddOrder(order);
        const auto end = Clock::now();

        latencies.push_back(
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<Nanoseconds>(
                    end - start).count()));
    }

    const auto batchEnd = Clock::now();

    const auto batchElapsedNs =
        std::chrono::duration_cast<Nanoseconds>(
            batchEnd - batchStart).count();

    const bool valid = book.ValidateBook();
    const std::size_t finalBookSize = book.Size();

    if (!valid || finalBookSize != expectedFinalBookSize)
    {
        throw std::runtime_error(
            "Measured order-book validation failed for scenario: " +
            scenario +
            ". Expected book size: " +
            std::to_string(expectedFinalBookSize) +
            ", actual book size: " +
            std::to_string(finalBookSize));
    }

    std::sort(latencies.begin(), latencies.end());

    std::uint64_t totalNs = 0;

    for (const auto latency : latencies)
    {
        totalNs += latency;
    }

    BenchmarkResult result;
    result.scenario = scenario;
    result.averageNs = static_cast<double>(totalNs) / static_cast<double>(latencies.size());
    result.p50Ns = Percentile(latencies, 50.0);
    result.p90Ns = Percentile(latencies, 90.0);
    result.p99Ns = Percentile(latencies, 99.0);
    result.p999Ns = Percentile(latencies, 99.9);
    result.throughput = batchElapsedNs > 0 ? static_cast<double>(orderCount) * 1'000'000'000.0 / static_cast<double>(batchElapsedNs) : 0.0;
    result.finalBookSize = book.Size();
    result.bookValid = valid;

    return result;
}

void PrintResult(const BenchmarkResult& result)
{
    std::cout << "\nScenario: " << result.scenario << '\n'
              << "Average latency: " << result.averageNs << " ns\n"
              << "p50 latency: " << result.p50Ns << " ns\n"
              << "p90 latency: " << result.p90Ns << " ns\n"
              << "p99 latency: " << result.p99Ns << " ns\n"
              << "p99.9 latency: " << result.p999Ns << " ns\n"
              << "Throughput: " << result.throughput
              << " operations/sec\n"
              << "Final book size: " << result.finalBookSize << '\n'
              << "Book valid: " << std::boolalpha
              << result.bookValid << '\n';
}

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;

    try
    {
        const auto singleLevel = RunBenchmark(
            "Single price level: consume entire book",
            ORDER_COUNT,
            1,
            10'001,
            0);

        const auto multiLevelFull = RunBenchmark(
            "100 price levels: consume entire book",
            ORDER_COUNT,
            100,
            10'100,
            0);

        const auto multiLevelBest = RunBenchmark(
            "100 price levels: consume best level only",
            ORDER_COUNT,
            100,
            10'000,
            198'000);

        std::cout << std::fixed << std::setprecision(2);

        PrintResult(singleLevel);
        PrintResult(multiLevelFull);
        PrintResult(multiLevelBest);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Benchmark failed: "
                  << exception.what() << '\n';

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}