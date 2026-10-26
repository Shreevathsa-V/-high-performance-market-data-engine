#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
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


// -------------------------------------------------------------
// Calculate percentile.
//
// Example:
//
// p50  -> median
// p99  -> 99% of observations are at or below this value
// p99.9 -> 99.9% of observations are at or below this value
// -------------------------------------------------------------

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

    const auto lower =
        static_cast<std::size_t>(index);

    const auto upper =
        std::min(
            lower + 1,
            samples.size() - 1);

    const double fraction =
        index - static_cast<double>(lower);

    return static_cast<double>(
               samples[lower]) +
           fraction *
               static_cast<double>(
                   samples[upper] -
                   samples[lower]);
}


// -------------------------------------------------------------
// Measure timer overhead.
//
// We execute the same timer operation repeatedly without doing
// any matching work.
//
// This gives us an estimate of the measurement cost.
// -------------------------------------------------------------

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


int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    constexpr std::size_t WARMUP_COUNT = 10'000;

    // ---------------------------------------------------------
    // Warm up the matching engine.
    //
    // Use a separate order book so these operations do not
    // affect the measured benchmark.
    // ---------------------------------------------------------

    {
        OrderBook warmupBook;

        for (std::size_t i = 0; i < WARMUP_COUNT; ++i)
        {
            auto sellOrder = std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Sell,
                10000,
                100);

            warmupBook.AddOrder(sellOrder);
        }

        for (std::size_t i = 0; i < WARMUP_COUNT; ++i)
        {
            auto buyOrder = std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(WARMUP_COUNT + i + 1),
                Side::Buy,
                10100,
                100);

            warmupBook.AddOrder(buyOrder);
        }

        if (!warmupBook.ValidateBook() ||
            warmupBook.Size() != 0)
        {
            std::cerr << "ERROR: Warm-up order-book validation failed.\n";
            return EXIT_FAILURE;
        }
    }

    // ---------------------------------------------------------
    // Create the order book.
    // ---------------------------------------------------------

    OrderBook orderBook;

    // ---------------------------------------------------------
    // Add resting SELL orders.
    //
    // These are created before measurement starts.
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


    std::cout
        << "Initial book size: "
        << orderBook.Size()
        << '\n';

    std::cout
        << "Initial book valid: "
        << std::boolalpha
        << orderBook.ValidateBook()
        << "\n\n";


    // ---------------------------------------------------------
    // Measure timer overhead.
    // ---------------------------------------------------------

    const std::uint64_t timerOverhead =
        MeasureTimerOverhead();

    std::cout
        << "Estimated timer overhead: "
        << timerOverhead
        << " ns\n";


    // ---------------------------------------------------------
    // Store one latency sample per matching operation.
    // ---------------------------------------------------------

    std::vector<std::uint64_t>
        latencies;

    latencies.reserve(ORDER_COUNT);


    // ---------------------------------------------------------
    // Measure each matching operation.
    //
    // Keep raw per-operation latency samples.
    // Measure total batch time separately for throughput.
    // ---------------------------------------------------------

    const auto batchStart = Clock::now();

    for (const auto& buyOrder : incomingOrders)
    {
        const auto start = Clock::now();

        orderBook.AddOrder(buyOrder);

        const auto end = Clock::now();

        const auto elapsed =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<Nanoseconds>(
                    end - start)
                    .count());

        // Store the raw measurement.
        // Do not subtract estimated timer overhead.
        latencies.push_back(elapsed);
    }

    const auto batchEnd = Clock::now();

    const auto batchElapsedNs =
        std::chrono::duration_cast<Nanoseconds>(
            batchEnd - batchStart)
            .count();


    // ---------------------------------------------------------
    // Validate final state.
    // ---------------------------------------------------------

    std::cout
        << "\nFinal book size: "
        << orderBook.Size()
        << '\n';

    const bool bookValid = orderBook.ValidateBook();

    std::cout
        << "Final book valid: "
        << std::boolalpha
        << bookValid
        << "\n\n";

    if (!bookValid)
    {
        std::cerr
            << "ERROR: Order-book validation failed after matching.\n";

        return EXIT_FAILURE;
    }


    // ---------------------------------------------------------
    // Calculate statistics.
    // ---------------------------------------------------------

    std::uint64_t total = 0;

    for (const auto latency : latencies)
    {
        total += latency;
    }

    const double average =
        static_cast<double>(total) /
        static_cast<double>(latencies.size());

    const double p50 =
        Percentile(latencies, 50.0);

    const double p90 =
        Percentile(latencies, 90.0);

    const double p99 =
        Percentile(latencies, 99.0);

    const double p999 =
        Percentile(latencies, 99.9);

    const auto minimum =
        *std::min_element(
            latencies.begin(),
            latencies.end());

    const auto maximum =
        *std::max_element(
            latencies.begin(),
            latencies.end());


    // ---------------------------------------------------------
    // Print results.
    // ---------------------------------------------------------

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "Matching latency distribution "
        << "(includes clock-call overhead)\n";

    std::cout
        << "--------------------------------\n";

    std::cout
        << "Samples: "
        << latencies.size()
        << '\n';

    std::cout
        << "Minimum: "
        << minimum
        << " ns\n";

    std::cout
        << "Average: "
        << average
        << " ns\n";

    std::cout
        << "p50:     "
        << p50
        << " ns\n";

    std::cout
        << "p90:     "
        << p90
        << " ns\n";

    std::cout
        << "p99:     "
        << p99
        << " ns\n";

    std::cout
        << "p99.9:   "
        << p999
        << " ns\n";

    std::cout
        << "Maximum: "
        << maximum
        << " ns\n";


    // ---------------------------------------------------------
    // Throughput.
    //
    // Note:
    // This is calculated from the sum of per-operation
    // measured latencies, so it is an approximate engine
    // throughput metric.
    // ---------------------------------------------------------

    const double totalSeconds = static_cast<double>(batchElapsedNs) / 1'000'000'000.0;

    const double throughput = totalSeconds > 0.0 ? static_cast<double>(ORDER_COUNT) / totalSeconds : 0.0;

    std::cout << "Approximate throughput: " << std::fixed << std::setprecision(2) << throughput << " orders/sec\n";

    return 0;
}