#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "feed/Protocol.h"
#include "feed/FeedHandler.h"
#include "feed/StreamProcessor.h"
#include "OrderBook.h"

namespace
{
    constexpr std::size_t WARMUP_MESSAGES = 5'000;
    constexpr std::size_t NUM_SAMPLES = 100'000;

    using Clock = std::chrono::steady_clock;
    using Nanoseconds = std::chrono::nanoseconds;

    void WriteBigEndian64(
        std::uint8_t* destination,
        std::uint64_t value)
    {
        for (int i = 7; i >= 0; --i)
        {
            destination[i] =
                static_cast<std::uint8_t>(value & 0xFF);

            value >>= 8;
        }
    }

    std::array<std::uint8_t, 29> CreateNewOrder(
        std::uint64_t orderId)
    {
        std::array<std::uint8_t, 29> message{};

        // Message type: NewOrder = 1.
        message[0] = 1;

        // Message length: 29 bytes, big-endian.
        message[1] = 0;
        message[2] = 29;

        // Order ID: bytes 3-10.
        WriteBigEndian64(message.data() + 3, orderId);

        // Side: Buy = 0.
        message[11] = 0;

        // Order type: GoodTillCancel = 0.
        message[12] = 0;

        // Price: 10000, bytes 13-20.
        WriteBigEndian64(message.data() + 13, 10000);

        // Quantity: 1, bytes 21-28.
        WriteBigEndian64(message.data() + 21, 1);

        return message;
    }

    std::array<std::uint8_t, 11> CreateCancelOrder(
        std::uint64_t orderId)
    {
        std::array<std::uint8_t, 11> message{};

        // Message type: CancelOrder = 2.
        message[0] = 2;

        // Message length: 11 bytes, big-endian.
        message[1] = 0;
        message[2] = 11;

        // Order ID: bytes 3-10.
        WriteBigEndian64(message.data() + 3, orderId);

        return message;
    }

    bool ProcessOneMessage(
        feed::StreamProcessor& processor,
        const std::uint8_t* data,
        std::size_t size)
    {
        const auto result = processor.ProcessBytes(data, size);

        return result.Ok()
            && result.messagesProcessed == 1
            && result.messagesRejected == 0
            && result.bufferedBytes == 0;
    }

    std::uint64_t MeasureMessage(
        feed::StreamProcessor& processor,
        const std::uint8_t* data,
        std::size_t size,
        bool& success)
    {
        const auto start = Clock::now();

        success = ProcessOneMessage(processor, data, size);

        const auto end = Clock::now();

        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<Nanoseconds>(
                end - start).count());
    }

    void PrintStatistics(
        const char* label,
        std::vector<std::uint64_t> samples)
    {
        if (samples.empty())
        {
            std::cout << label << ": no samples\n";
            return;
        }

        std::sort(samples.begin(), samples.end());

        long double total = 0;

        for (const auto sample : samples)
        {
            total += sample;
        }

        const double average =
            static_cast<double>(total / samples.size());

        const auto percentile =
            [&samples](double p) -> std::uint64_t
            {
                const auto index = static_cast<std::size_t>(
                    p * samples.size());

                const auto boundedIndex =
                    std::min(index, samples.size() - 1);

                return samples[boundedIndex];
            };

        std::cout
            << "\n" << label << '\n'
            << "Samples: " << samples.size() << '\n'
            << "Average: " << std::fixed
            << std::setprecision(2)
            << average << " ns\n"
            << "p50: " << percentile(0.50) << " ns\n"
            << "p90: " << percentile(0.90) << " ns\n"
            << "p99: " << percentile(0.99) << " ns\n"
            << "p99.9: " << percentile(0.999) << " ns\n"
            << "Maximum: " << samples.back() << " ns\n";
    }
}

int main()
{
    // Disable optional engine logging for the benchmark.
    OrderBook orderBook(false);

    feed::FeedHandler handler(orderBook);
    feed::StreamProcessor processor(handler);

    std::vector<std::uint64_t> newOrderSamples;
    std::vector<std::uint64_t> cancelSamples;

    newOrderSamples.reserve(NUM_SAMPLES);
    cancelSamples.reserve(NUM_SAMPLES);

    std::uint64_t failedMessages = 0;

    std::cout
        << "Feed Pipeline Latency Benchmark\n"
        << "===============================\n"
        << "Warm-up messages: "
        << WARMUP_MESSAGES * 2 << '\n'
        << "Measured order/cancel pairs: "
        << NUM_SAMPLES << '\n'
        << "TCP overhead: excluded\n";

    // Warm up the processing pipeline.
    for (std::uint64_t i = 1; i <= WARMUP_MESSAGES; ++i)
    {
        const auto newOrder = CreateNewOrder(i);
        const auto cancelOrder = CreateCancelOrder(i);

        if (!ProcessOneMessage(
                processor,
                newOrder.data(),
                newOrder.size()))
        {
            ++failedMessages;
        }

        if (!ProcessOneMessage(
                processor,
                cancelOrder.data(),
                cancelOrder.size()))
        {
            ++failedMessages;
        }
    }

    // Measure new-order and cancellation processing separately.
    for (std::uint64_t i = 0; i < NUM_SAMPLES; ++i)
    {
        const std::uint64_t orderId = 1'000'000 + i;

        const auto newOrder = CreateNewOrder(orderId);
        const auto cancelOrder = CreateCancelOrder(orderId);

        bool orderSuccess = false;

        const auto orderLatency = MeasureMessage(
            processor,
            newOrder.data(),
            newOrder.size(),
            orderSuccess);

        newOrderSamples.push_back(orderLatency);

        if (!orderSuccess)
        {
            ++failedMessages;
        }

        bool cancelSuccess = false;

        const auto cancelLatency = MeasureMessage(
            processor,
            cancelOrder.data(),
            cancelOrder.size(),
            cancelSuccess);

        cancelSamples.push_back(cancelLatency);

        if (!cancelSuccess)
        {
            ++failedMessages;
        }
    }

    std::cout
        << "\nFailed messages: "
        << failedMessages << '\n';

    PrintStatistics(
        "New Order Processing Latency",
        newOrderSamples);

    PrintStatistics(
        "Cancellation Processing Latency",
        cancelSamples);

    return failedMessages == 0 ? 0 : 1;
}