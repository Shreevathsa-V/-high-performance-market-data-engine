#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
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
    double average;
    double p50;
    double p99;
    double p999;
    std::uint64_t minimum;
    std::uint64_t maximum;
};

double Percentile(
    std::vector<std::uint64_t>& samples,
    double percentile)
{
    if (samples.empty())
    {
        return 0.0;
    }

    std::sort(
        samples.begin(),
        samples.end());

    const double index =
        (percentile / 100.0) *
        static_cast<double>(samples.size() - 1);

    const std::size_t lower =
        static_cast<std::size_t>(index);

    const std::size_t upper =
        std::min(
            lower + 1,
            samples.size() - 1);

    const double fraction =
        index - static_cast<double>(lower);

    return static_cast<double>(samples[lower]) +
           fraction *
               static_cast<double>(
                   samples[upper] -
                   samples[lower]);
}

std::uint64_t MeasureTimerOverhead()
{
    constexpr std::size_t SAMPLE_COUNT = 100'000;

    std::uint64_t total = 0;

    for (std::size_t i = 0;
         i < SAMPLE_COUNT;
         ++i)
    {
        const auto start = Clock::now();
        const auto end = Clock::now();

        total += static_cast<std::uint64_t>(
            std::chrono::duration_cast<Nanoseconds>(
                end - start)
                .count());
    }

    return total / SAMPLE_COUNT;
}

BenchmarkResult RunBenchmark(
    bool eventLoggingEnabled,
    std::uint64_t timerOverhead)
{
    constexpr std::size_t ORDER_COUNT = 100'000;

    OrderBook orderBook(eventLoggingEnabled);

    if (eventLoggingEnabled)
    {
        orderBook.ReserveEventLog(ORDER_COUNT);
    }

    // ---------------------------------------------------------
    // Create resting SELL orders.
    // ---------------------------------------------------------

    for (std::size_t i = 0;
         i < ORDER_COUNT;
         ++i)
    {
        auto sellOrder =
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Sell,
                10000,
                100);

        orderBook.AddOrder(sellOrder);
    }

    // ---------------------------------------------------------
    // Pre-create incoming BUY orders.
    // ---------------------------------------------------------

    std::vector<std::shared_ptr<Order>>
        incomingOrders;

    incomingOrders.reserve(ORDER_COUNT);

    for (std::size_t i = 0;
         i < ORDER_COUNT;
         ++i)
    {
        incomingOrders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(
                    ORDER_COUNT + i + 1),
                Side::Buy,
                10100,
                100));
    }

    // ---------------------------------------------------------
    // Measure each matching operation.
    // ---------------------------------------------------------

    std::vector<std::uint64_t>
        latencies;

    latencies.reserve(ORDER_COUNT);

    for (const auto& buyOrder : incomingOrders)
    {
        const auto start = Clock::now();

        orderBook.AddOrder(buyOrder);

        const auto end = Clock::now();

        const auto elapsed =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<
                    Nanoseconds>(
                        end - start)
                    .count());

        const std::uint64_t adjusted =
            elapsed > timerOverhead
                ? elapsed - timerOverhead
                : 0;

        latencies.push_back(adjusted);
    }

    // ---------------------------------------------------------
    // Make sure the benchmark didn't corrupt the book.
    // ---------------------------------------------------------

    if (!orderBook.ValidateBook())
    {
        std::cerr
            << "ERROR: Book validation failed.\n";

        std::exit(1);
    }

    // ---------------------------------------------------------
    // Calculate statistics.
    // ---------------------------------------------------------

    std::uint64_t total = 0;

    for (const auto latency : latencies)
    {
        total += latency;
    }

    BenchmarkResult result{};

    result.average =
        static_cast<double>(total) /
        static_cast<double>(latencies.size());

    result.p50 =
        Percentile(latencies, 50.0);

    result.p99 =
        Percentile(latencies, 99.0);

    result.p999 =
        Percentile(latencies, 99.9);

    result.minimum =
        *std::min_element(
            latencies.begin(),
            latencies.end());

    result.maximum =
        *std::max_element(
            latencies.begin(),
            latencies.end());

    return result;
}

void PrintResult(
    const BenchmarkResult& result)
{
    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "Minimum: "
        << result.minimum
        << " ns\n";

    std::cout
        << "Average: "
        << result.average
        << " ns\n";

    std::cout
        << "p50:     "
        << result.p50
        << " ns\n";

    std::cout
        << "p99:     "
        << result.p99
        << " ns\n";

    std::cout
        << "p99.9:   "
        << result.p999
        << " ns\n";

    std::cout
        << "Maximum: "
        << result.maximum
        << " ns\n";
}

int main()
{
    const std::uint64_t timerOverhead =
        MeasureTimerOverhead();

    std::cout
        << "Timer overhead: "
        << timerOverhead
        << " ns\n\n";


    // ---------------------------------------------------------
    // Benchmark WITH event logging.
    // ---------------------------------------------------------

    std::cout
        << "=== Event Logging ENABLED ===\n";

    const BenchmarkResult withLogging =
        RunBenchmark(
            true,
            timerOverhead);

    PrintResult(withLogging);


    // ---------------------------------------------------------
    // Benchmark WITHOUT event logging.
    // ---------------------------------------------------------

    std::cout
        << "\n=== Event Logging DISABLED ===\n";

    const BenchmarkResult withoutLogging =
        RunBenchmark(
            false,
            timerOverhead);

    PrintResult(withoutLogging);


    // ---------------------------------------------------------
    // Compare average latency.
    // ---------------------------------------------------------

    std::cout
        << "\n=== Comparison ===\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "Average with logging: "
        << withLogging.average
        << " ns\n";

    std::cout
        << "Average without logging: "
        << withoutLogging.average
        << " ns\n";

    if (withLogging.average >
        withoutLogging.average)
    {
        const double difference =
            withLogging.average -
            withoutLogging.average;

        const double percentage =
            (difference /
             withoutLogging.average) *
            100.0;

        std::cout
            << "Logging overhead: "
            << difference
            << " ns/order ("
            << percentage
            << "%)\n";
    }

    return 0;
}