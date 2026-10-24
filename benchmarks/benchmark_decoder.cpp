#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include "feed/Decoder.h"
#include "feed/Protocol.h"

using Clock = std::chrono::steady_clock;

namespace
{
    constexpr std::size_t ITERATIONS = 1'000'000;

    // Write a 16-bit integer in big-endian order.
    void WriteBigEndian16(
        std::uint8_t* data,
        std::uint16_t value)
    {
        data[0] = static_cast<std::uint8_t>(value >> 8);
        data[1] = static_cast<std::uint8_t>(value);
    }

    // Write a 64-bit integer in big-endian order.
    void WriteBigEndian64(
        std::uint8_t* data,
        std::uint64_t value)
    {
        for (int i = 7; i >= 0; --i)
        {
            data[i] = static_cast<std::uint8_t>(value);
            value >>= 8;
        }
    }

    std::vector<std::uint8_t> MakeNewOrderMessage()
    {
        std::vector<std::uint8_t> message(
            feed::NEW_ORDER_MESSAGE_SIZE, 0);

        message[0] =
            static_cast<std::uint8_t>(
                feed::MessageType::NewOrder);

        WriteBigEndian16(
            message.data() + 1,
            static_cast<std::uint16_t>(
                feed::NEW_ORDER_MESSAGE_SIZE));

        std::size_t offset = feed::HEADER_SIZE;

        WriteBigEndian64(message.data() + offset, 123456);
        offset += 8;

        message[offset++] =
            static_cast<std::uint8_t>(feed::Side::Buy);

        message[offset++] =
            static_cast<std::uint8_t>(feed::OrderType::GoodTillCancel);

        WriteBigEndian64(message.data() + offset, 10000);
        offset += 8;

        WriteBigEndian64(message.data() + offset, 10);

        return message;
    }

    std::vector<std::uint8_t> MakeCancelOrderMessage()
    {
        std::vector<std::uint8_t> message(
            feed::CANCEL_ORDER_MESSAGE_SIZE, 0);

        message[0] =
            static_cast<std::uint8_t>(
                feed::MessageType::CancelOrder);

        WriteBigEndian16(
            message.data() + 1,
            static_cast<std::uint16_t>(
                feed::CANCEL_ORDER_MESSAGE_SIZE));

        WriteBigEndian64(
            message.data() + feed::HEADER_SIZE,
            123456);

        return message;
    }

    template <typename Function>
    void RunBenchmark(
        const char* name,
        Function function)
    {
        // Warm up the code before measuring it.
        for (std::size_t i = 0; i < 100'000; ++i)
        {
            function();
        }

        std::uint64_t successfulDecodes = 0;

        const auto start = Clock::now();

        for (std::size_t i = 0; i < ITERATIONS; ++i)
        {
            successfulDecodes += function() ? 1 : 0;
        }

        const auto finish = Clock::now();

        const double elapsedSeconds =
            std::chrono::duration<double>(
                finish - start).count();

        const double throughput =
            static_cast<double>(ITERATIONS) /
            elapsedSeconds;

        const double nanosecondsPerDecode =
            elapsedSeconds * 1'000'000'000.0 /
            static_cast<double>(ITERATIONS);

        std::cout
            << name << '\n'
            << "  Iterations:          "
            << ITERATIONS << '\n'
            << "  Successful decodes:  "
            << successfulDecodes << '\n'
            << "  Elapsed:             "
            << std::fixed << std::setprecision(6)
            << elapsedSeconds << " seconds\n"
            << "  Time per decode:     "
            << std::setprecision(2)
            << nanosecondsPerDecode << " ns\n"
            << "  Throughput:          "
            << std::setprecision(0)
            << throughput << " messages/sec\n\n";

        if (successfulDecodes != ITERATIONS)
        {
            std::cerr
                << "ERROR: Not all decodes succeeded.\n";

            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    const auto newOrder = MakeNewOrderMessage();
    const auto cancelOrder = MakeCancelOrderMessage();

    std::cout
        << "DECODER MICROBENCHMARK\n"
        << "======================\n"
        << "Compiler optimization should be enabled.\n\n";

    RunBenchmark(
        "New-order decoder",
        [&]()
        {
            const auto result =
                feed::DecodeNewOrder(
                    newOrder.data(),
                    newOrder.size());

            return result.Ok() &&
                   result.message.orderId == 123456 &&
                   result.message.price == 10000 &&
                   result.message.quantity == 10;
        });

    RunBenchmark(
        "Cancel-order decoder",
        [&]()
        {
            feed::DecodeError error =
                feed::DecodeError::None;

            const auto result =
                feed::DecodeCancelOrder(
                    cancelOrder.data(),
                    cancelOrder.size(),
                    error);

            return error == feed::DecodeError::None &&
                   result.orderId == 123456;
        });

    return EXIT_SUCCESS;
}