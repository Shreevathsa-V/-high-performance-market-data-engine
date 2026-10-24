#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>

#include "feed/FeedHandler.h"
#include "feed/Protocol.h"
#include "feed/StreamProcessor.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t ITERATIONS = 500'000;
    constexpr std::uint64_t ORDER_ID = 123456;

    using OrderBuffer =
        std::array<std::uint8_t, feed::NEW_ORDER_MESSAGE_SIZE>;

    using CancelBuffer =
        std::array<std::uint8_t, feed::CANCEL_ORDER_MESSAGE_SIZE>;

    struct BenchmarkResult
    {
        std::uint64_t messagesProcessed = 0;
        std::uint64_t messagesRejected = 0;
        bool valid = true;
    };

    void WriteBigEndian16(
        std::uint8_t* destination,
        std::uint16_t value)
    {
        destination[0] =
            static_cast<std::uint8_t>(value >> 8);

        destination[1] =
            static_cast<std::uint8_t>(value);
    }

    void WriteBigEndian64(
        std::uint8_t* destination,
        std::uint64_t value)
    {
        for (int i = 7; i >= 0; --i)
        {
            destination[i] =
                static_cast<std::uint8_t>(value);

            value >>= 8;
        }
    }

    OrderBuffer MakeOrderMessage()
    {
        OrderBuffer message{};

        message[0] =
            static_cast<std::uint8_t>(
                feed::MessageType::NewOrder);

        WriteBigEndian16(
            message.data() + feed::MESSAGE_TYPE_SIZE,
            static_cast<std::uint16_t>(
                feed::NEW_ORDER_MESSAGE_SIZE));

        std::size_t offset = feed::HEADER_SIZE;

        WriteBigEndian64(
            message.data() + offset,
            ORDER_ID);

        offset += feed::ORDER_ID_SIZE;

        message[offset++] =
            static_cast<std::uint8_t>(
                feed::Side::Buy);

        message[offset++] =
            static_cast<std::uint8_t>(
                feed::OrderType::GoodTillCancel);

        WriteBigEndian64(
            message.data() + offset,
            10000);

        offset += feed::PRICE_SIZE;

        WriteBigEndian64(
            message.data() + offset,
            10);

        return message;
    }

    CancelBuffer MakeCancelMessage()
    {
        CancelBuffer message{};

        message[0] =
            static_cast<std::uint8_t>(
                feed::MessageType::CancelOrder);

        WriteBigEndian16(
            message.data() + feed::MESSAGE_TYPE_SIZE,
            static_cast<std::uint16_t>(
                feed::CANCEL_ORDER_MESSAGE_SIZE));

        WriteBigEndian64(
            message.data() + feed::HEADER_SIZE,
            ORDER_ID);

        return message;
    }

    void RecordResult(const feed::StreamResult& result, BenchmarkResult& totals)
    {
        if (!result.Ok())
        {
            totals.valid = false;
        }

        totals.messagesProcessed +=
            result.messagesProcessed;

        totals.messagesRejected +=
            result.messagesRejected;
    }

    template <typename Function>
    void RunBenchmark(
        const char* name,
        Function benchmark)
    {
        // Warm-up: execute the workload before measuring it.
        const BenchmarkResult warmup = benchmark(10'000);

        if (!warmup.valid ||
            warmup.messagesProcessed != 20'000 ||
            warmup.messagesRejected != 0)
        {
            std::cerr
                << "Warm-up failed: "
                << name << '\n';

            std::exit(EXIT_FAILURE);
        }

        const auto start = Clock::now();

        const BenchmarkResult result =
            benchmark(ITERATIONS);

        const auto finish = Clock::now();

        const double elapsedSeconds =
            std::chrono::duration<double>(
                finish - start).count();

        const std::uint64_t expectedMessages =
            ITERATIONS * 2;

        if (!result.valid ||
            result.messagesProcessed != expectedMessages ||
            result.messagesRejected != 0)
        {
            std::cerr
                << "Correctness validation failed: "
                << name << '\n'
                << "Processed: "
                << result.messagesProcessed << '\n'
                << "Rejected: "
                << result.messagesRejected << '\n';

            std::exit(EXIT_FAILURE);
        }

        const double throughput =
            static_cast<double>(
                result.messagesProcessed) /
            elapsedSeconds;

        const double nsPerMessage =
            elapsedSeconds * 1'000'000'000.0 /
            static_cast<double>(
                result.messagesProcessed);

        std::cout
            << name << '\n'
            << "  Order/cancel pairs: "
            << ITERATIONS << '\n'
            << "  Messages processed: "
            << result.messagesProcessed << '\n'
            << "  Messages rejected:  "
            << result.messagesRejected << '\n'
            << "  Elapsed:            "
            << std::fixed << std::setprecision(6)
            << elapsedSeconds << " seconds\n"
            << "  Time per message:    "
            << std::setprecision(2)
            << nsPerMessage << " ns\n"
            << "  Throughput:          "
            << std::setprecision(0)
            << throughput << " messages/sec\n\n";
    }

    BenchmarkResult BenchmarkSeparateMessages(
        std::size_t iterations)
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto order = MakeOrderMessage();
        const auto cancel = MakeCancelMessage();

        BenchmarkResult totals;

        for (std::size_t i = 0; i < iterations; ++i)
        {
            const auto orderResult =
                processor.ProcessBytes(
                    order.data(),
                    order.size());

            RecordResult(orderResult, totals);

            const auto cancelResult =
                processor.ProcessBytes(
                    cancel.data(),
                    cancel.size());

            RecordResult(cancelResult, totals);
        }

        if (!book.Empty() || !book.ValidateBook())
        {
            totals.valid = false;
        }

        return totals;
    }

    BenchmarkResult BenchmarkCombinedMessages(
        std::size_t iterations)
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto order = MakeOrderMessage();
        const auto cancel = MakeCancelMessage();

        std::array<
            std::uint8_t,
            feed::NEW_ORDER_MESSAGE_SIZE +
                feed::CANCEL_ORDER_MESSAGE_SIZE> combined{};

        std::copy(
            order.begin(),
            order.end(),
            combined.begin());

        std::copy(
            cancel.begin(),
            cancel.end(),
            combined.begin() + order.size());

        BenchmarkResult totals;

        for (std::size_t i = 0; i < iterations; ++i)
        {
            const auto result =
                processor.ProcessBytes(
                    combined.data(),
                    combined.size());

            RecordResult(result, totals);
        }

        if (!book.Empty() || !book.ValidateBook())
        {
            totals.valid = false;
        }

        return totals;
    }

    BenchmarkResult BenchmarkFragmentedMessages(
        std::size_t iterations)
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto order = MakeOrderMessage();
        const auto cancel = MakeCancelMessage();

        // The order arrives in two TCP chunks.
        constexpr std::size_t FIRST_CHUNK_SIZE = 10;

        BenchmarkResult totals;

        for (std::size_t i = 0; i < iterations; ++i)
        {
            const auto first =
                processor.ProcessBytes(
                    order.data(),
                    FIRST_CHUNK_SIZE);

            RecordResult(first, totals);

            const auto second =
                processor.ProcessBytes(
                    order.data() + FIRST_CHUNK_SIZE,
                    order.size() - FIRST_CHUNK_SIZE);

            RecordResult(second, totals);

            const auto third =
                processor.ProcessBytes(
                    cancel.data(),
                    cancel.size());

            RecordResult(third, totals);
        }

        if (!book.Empty() ||
            !book.ValidateBook() ||
            processor.BufferedBytes() != 0)
        {
            totals.valid = false;
        }

        return totals;
    }
}

int main()
{
    std::cout
        << "STREAM PROCESSOR MICROBENCHMARK\n"
        << "================================\n"
        << "Measured order/cancel pairs: "
        << ITERATIONS << '\n'
        << "Expected messages per scenario: "
        << ITERATIONS * 2 << "\n\n";

    RunBenchmark(
        "1. Complete messages in separate calls",
        BenchmarkSeparateMessages);

    RunBenchmark(
        "2. Order and cancellation in one buffer",
        BenchmarkCombinedMessages);

    RunBenchmark(
        "3. Order fragmented across two calls",
        BenchmarkFragmentedMessages);

    std::cout
        << "All scenarios passed correctness validation.\n";

    return EXIT_SUCCESS;
}