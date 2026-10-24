#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>

#include "Order.h"
#include "OrderBook.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

using Clock = std::chrono::steady_clock;

struct BenchmarkResult
{
    double nsPerOrder;
    double ordersPerSecond;
    std::size_t finalBookSize;
    bool bookValid;
    std::size_t eventCount;
};

BenchmarkResult RunBenchmark(bool eventLoggingEnabled)
{
    constexpr std::size_t ORDER_COUNT = 100'000;

    OrderBook orderBook(eventLoggingEnabled);

    if (eventLoggingEnabled)
    {
        orderBook.ReserveEventLog(ORDER_COUNT * 2);
    }

    // Create resting sell orders outside the timed region.
    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        auto sellOrder = std::make_shared<Order>(
            OrderType::GoodTillCancel,
            static_cast<OrderId>(i + 1),
            Side::Sell,
            10000,
            100);

        orderBook.AddOrder(sellOrder);
    }

    // Pre-create incoming buy orders outside the timed region.
    std::vector<std::shared_ptr<Order>> incomingOrders;
    incomingOrders.reserve(ORDER_COUNT);

    for (std::size_t i = 0; i < ORDER_COUNT; ++i)
    {
        incomingOrders.push_back(
            std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(ORDER_COUNT + i + 1),
                Side::Buy,
                10100,
                100));
    }

    if (!orderBook.ValidateBook())
    {
        std::cerr << "Initial book validation failed.\n";
        return {0.0, 0.0, orderBook.Size(), false, 0};
    }

    // Measure the complete matching batch.
    const auto start = Clock::now();

    for (const auto& buyOrder : incomingOrders)
    {
        orderBook.AddOrder(buyOrder);
    }

    const auto end = Clock::now();

    const auto elapsedNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start)
            .count();

    const double nsPerOrder =
        static_cast<double>(elapsedNs) /
        static_cast<double>(ORDER_COUNT);

    const double ordersPerSecond =
        elapsedNs > 0
            ? static_cast<double>(ORDER_COUNT) *
                  1'000'000'000.0 /
                  static_cast<double>(elapsedNs)
            : 0.0;

    const bool bookValid = orderBook.ValidateBook();

    return {
        nsPerOrder,
        ordersPerSecond,
        orderBook.Size(),
        bookValid,
        orderBook.GetEventLog().size()
    };
}

int main()
{
    constexpr int WARMUP_TRIALS = 2;
    constexpr int MEASURED_TRIALS = 9;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "=== Event Logging Comparison ===\n\n";

    // -----------------------------------------------------
    // Warm-up phase.
    // Both configurations execute before measurements begin.
    // -----------------------------------------------------

    std::cout << "Warm-up phase...\n";

    for (int i = 0; i < WARMUP_TRIALS; ++i)
    {
        RunBenchmark(true);
        RunBenchmark(false);
    }

    std::cout << "Warm-up completed.\n\n";

    std::vector<double> enabledLatencies;
    std::vector<double> disabledLatencies;

    std::vector<double> enabledThroughputs;
    std::vector<double> disabledThroughputs;

    enabledLatencies.reserve(MEASURED_TRIALS);
    disabledLatencies.reserve(MEASURED_TRIALS);

    enabledThroughputs.reserve(MEASURED_TRIALS);
    disabledThroughputs.reserve(MEASURED_TRIALS);

    // -----------------------------------------------------
    // Measurement phase.
    // Alternate execution order between trials.
    // -----------------------------------------------------

    for (int trial = 0; trial < MEASURED_TRIALS; ++trial)
    {
        BenchmarkResult enabled{};
        BenchmarkResult disabled{};

        std::cout << "Trial " << (trial + 1) << '\n';

        if (trial % 2 == 0)
        {
            enabled = RunBenchmark(true);
            disabled = RunBenchmark(false);
        }
        else
        {
            disabled = RunBenchmark(false);
            enabled = RunBenchmark(true);
        }

        const bool correct =
            enabled.bookValid &&
            disabled.bookValid &&
            enabled.finalBookSize == 0 &&
            disabled.finalBookSize == 0 &&
            enabled.eventCount == 200000 &&
            disabled.eventCount == 0;

        std::cout << "Logging enabled:  "
                  << enabled.nsPerOrder << " ns/order, "
                  << enabled.ordersPerSecond << " orders/sec\n";

        std::cout << "Logging disabled: "
                  << disabled.nsPerOrder << " ns/order, "
                  << disabled.ordersPerSecond << " orders/sec\n";

        std::cout << "Correctness: "
                  << std::boolalpha << correct << "\n\n";

        if (!correct)
        {
            std::cerr << "Correctness check failed.\n";
            return 1;
        }

        enabledLatencies.push_back(enabled.nsPerOrder);
        disabledLatencies.push_back(disabled.nsPerOrder);

        enabledThroughputs.push_back(enabled.ordersPerSecond);
        disabledThroughputs.push_back(disabled.ordersPerSecond);
    }

    // -----------------------------------------------------
    // Calculate medians.
    // -----------------------------------------------------

    auto median = [](std::vector<double> values)
    {
        std::sort(values.begin(), values.end());

        const std::size_t n = values.size();

        if (n % 2 == 1)
        {
            return values[n / 2];
        }

        return (values[n / 2 - 1] + values[n / 2]) / 2.0;
    };

    const double enabledLatencyMedian =
        median(enabledLatencies);

    const double disabledLatencyMedian =
        median(disabledLatencies);

    const double enabledThroughputMedian =
        median(enabledThroughputs);

    const double disabledThroughputMedian =
        median(disabledThroughputs);

    const double latencyDifference =
        enabledLatencyMedian > 0.0
            ? (enabledLatencyMedian - disabledLatencyMedian) /
                  enabledLatencyMedian * 100.0
            : 0.0;

    const double throughputDifference =
        enabledThroughputMedian > 0.0
            ? (disabledThroughputMedian - enabledThroughputMedian) /
                  enabledThroughputMedian * 100.0
            : 0.0;

    // -----------------------------------------------------
    // Print summary.
    // -----------------------------------------------------

    std::cout << "=== Median-Based Summary ===\n\n";

    std::cout << "Measured trials: "
              << MEASURED_TRIALS << '\n';

    std::cout << "Logging enabled median latency:  "
              << enabledLatencyMedian << " ns/order\n";

    std::cout << "Logging disabled median latency: "
              << disabledLatencyMedian << " ns/order\n";

    std::cout << "Median latency difference: "
              << latencyDifference << "%\n\n";

    std::cout << "Logging enabled median throughput:  "
              << enabledThroughputMedian << " orders/sec\n";

    std::cout << "Logging disabled median throughput: "
              << disabledThroughputMedian << " orders/sec\n";

    std::cout << "Median throughput difference: "
              << throughputDifference << "%\n\n";

    std::cout << "All benchmark correctness checks passed.\n";

    return 0;
}