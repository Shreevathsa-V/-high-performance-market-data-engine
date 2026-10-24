#include <array>
#include <bit>
#include <cstdint>
#include <iostream>

#include "feed/FeedHandler.h"
#include "feed/Protocol.h"

namespace
{
    using MessageBuffer =
        std::array<
            std::uint8_t,
            feed::NEW_ORDER_MESSAGE_SIZE>;

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

    MessageBuffer CreateOrderMessage(
        std::uint64_t orderId,
        feed::Side side,
        std::int64_t price,
        std::uint64_t quantity)
    {
        MessageBuffer data{};

        std::size_t offset = 0;

        data[offset++] = static_cast<std::uint8_t>(feed::MessageType::NewOrder);

        WriteBigEndian16(data.data() + offset, static_cast<std::uint16_t>(feed::NEW_ORDER_MESSAGE_SIZE));

        offset += feed::MESSAGE_LENGTH_SIZE;

        WriteBigEndian64(data.data() + offset, orderId);

        offset += feed::ORDER_ID_SIZE;

        data[offset++] = static_cast<std::uint8_t>(side);

        data[offset++] = static_cast<std::uint8_t>(feed::OrderType::GoodTillCancel);

        WriteBigEndian64(
            data.data() + offset,
            std::bit_cast<std::uint64_t>(price)
        );

        offset += feed::PRICE_SIZE;

        WriteBigEndian64(data.data() + offset, quantity);

        return data;
    }

        using CancelMessageBuffer = std::array<std::uint8_t, feed::CANCEL_ORDER_MESSAGE_SIZE>;

        CancelMessageBuffer CreateCancelMessage(std::uint64_t orderId)
        {
            CancelMessageBuffer data{};

            data[0] = static_cast<std::uint8_t>(feed::MessageType::CancelOrder);

            WriteBigEndian16(
                data.data() + feed::MESSAGE_TYPE_SIZE,
                static_cast<std::uint16_t>(feed::CANCEL_ORDER_MESSAGE_SIZE)
            );

            WriteBigEndian64(
                data.data() + feed::HEADER_SIZE,
                orderId
            );

            return data;
        }
}

int main()
{
    int failures = 0;

    OrderBook orderBook(false);
    feed::FeedHandler handler(orderBook);

    // -----------------------------------------------------
    // TEST 1: A valid order reaches the matching engine.
    // -----------------------------------------------------

    {
        const auto message = CreateOrderMessage(
            1001,
            feed::Side::Buy,
            10000,
            50);

        const auto result =
            handler.ProcessMessage(
                message.data(),
                message.size());

        const bool passed =
            result.Ok() &&
            orderBook.Size() == 1 &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 1 PASSED: Order reaches matching engine\n";
        }
        else
        {
            std::cout
                << "TEST 1 FAILED: Order reaches matching engine\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 2: An invalid message does not alter the book.
    // -----------------------------------------------------

    {
        const std::size_t originalSize = orderBook.Size();

        auto message = CreateOrderMessage(
            1002,
            feed::Side::Buy,
            10000,
            50);

        // Corrupt the message type.
        message[0] = 99;

        const auto result =
            handler.ProcessMessage(
                message.data(),
                message.size());

        const bool passed =
            !result.Ok() &&
            result.error == feed::FeedError::DecodeFailed &&
            orderBook.Size() == originalSize &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 2 PASSED: Invalid message rejected safely\n";
        }
        else
        {
            std::cout
                << "TEST 2 FAILED: Invalid message rejected safely\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 3: Incoming sell order matches the resting buy.
    // -----------------------------------------------------

    {
        const auto message = CreateOrderMessage(
            1003,
            feed::Side::Sell,
            10000,
            30);

        const auto result =
            handler.ProcessMessage(
                message.data(),
                message.size());

        const bool passed =
            result.Ok() &&
            result.orderResult.trades.size() == 1 &&
            result.orderResult.trades[0].bid.orderId == 1001 &&
            result.orderResult.trades[0].ask.orderId == 1003 &&
            result.orderResult.trades[0].bid.quantity == 30 &&
            orderBook.Size() == 1 &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 3 PASSED: Decoded orders match correctly\n";
        }
        else
        {
            std::cout
                << "TEST 3 FAILED: Decoded orders match correctly\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 4: An invalid order ID is rejected by the decoder.
    // -----------------------------------------------------

    {
        auto message = CreateOrderMessage(
            0,
            feed::Side::Buy,
            10000,
            50);

        const std::size_t originalSize = orderBook.Size();

        const auto result =
            handler.ProcessMessage(
                message.data(),
                message.size());

        const bool passed =
            !result.Ok() &&
            result.decodeError ==
                feed::DecodeError::InvalidOrderId &&
            orderBook.Size() == originalSize &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 4 PASSED: Invalid order ID rejected\n";
        }
        else
        {
            std::cout
                << "TEST 4 FAILED: Invalid order ID rejected\n";

            ++failures;
        }
    }

        // TEST 5: Cancel an existing order through the feed handler.
    {
        const auto newOrder = CreateOrderMessage(
            2001,
            feed::Side::Buy,
            9500,
            25);

        const auto addResult =
            handler.ProcessMessage(
                newOrder.data(),
                newOrder.size());

        const auto cancelMessage =
            CreateCancelMessage(2001);

        const auto cancelResult =
            handler.ProcessMessage(
                cancelMessage.data(),
                cancelMessage.size());

        const bool passed =
            addResult.Ok() &&
            cancelResult.Ok() &&
            cancelResult.cancellationProcessed &&
            cancelResult.orderCancelled &&
            orderBook.Size() == 1 &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 5 PASSED: Existing order cancelled through feed handler\n";
        }
        else
        {
            std::cout
                << "TEST 5 FAILED: Existing order cancelled through feed handler\n";

            ++failures;
        }
    }

    // TEST 6: Cancelling the same order twice is safe.
    {
        const auto newOrder = CreateOrderMessage(
            2002,
            feed::Side::Buy,
            9400,
            20);

        handler.ProcessMessage(
            newOrder.data(),
            newOrder.size());

        const auto cancelMessage =
            CreateCancelMessage(2002);

        const auto firstCancel =
            handler.ProcessMessage(
                cancelMessage.data(),
                cancelMessage.size());

        const auto secondCancel =
            handler.ProcessMessage(
                cancelMessage.data(),
                cancelMessage.size());

        const bool passed =
            firstCancel.Ok() &&
            firstCancel.orderCancelled &&
            secondCancel.Ok() &&
            secondCancel.cancellationProcessed &&
            !secondCancel.orderCancelled &&
            orderBook.Size() == 1 &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 6 PASSED: Repeated cancellation handled safely\n";
        }
        else
        {
            std::cout
                << "TEST 6 FAILED: Repeated cancellation handled safely\n";

            ++failures;
        }
    }

    // TEST 7: Cancelling an unknown order is safe.
    {
        const auto cancelMessage =
            CreateCancelMessage(999999);

        const std::size_t originalSize =
            orderBook.Size();

        const auto result =
            handler.ProcessMessage(
                cancelMessage.data(),
                cancelMessage.size());

        const bool passed =
            result.Ok() &&
            result.cancellationProcessed &&
            !result.orderCancelled &&
            orderBook.Size() == originalSize &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 7 PASSED: Unknown order cancellation handled safely\n";
        }
        else
        {
            std::cout
                << "TEST 7 FAILED: Unknown order cancellation handled safely\n";

            ++failures;
        }
    }

    // TEST 8: Malformed cancellation must not modify the book.
    {
        auto cancelMessage =
            CreateCancelMessage(1001);

        // Corrupt the encoded message length.
        WriteBigEndian16(
            cancelMessage.data() + feed::MESSAGE_TYPE_SIZE,
            10);

        const std::size_t originalSize =
            orderBook.Size();

        const auto result =
            handler.ProcessMessage(
                cancelMessage.data(),
                cancelMessage.size());

        const bool passed =
            !result.Ok() &&
            result.error == feed::FeedError::DecodeFailed &&
            result.decodeError ==
                feed::DecodeError::InvalidMessageLength &&
            orderBook.Size() == originalSize &&
            orderBook.ValidateBook();

        if (passed)
        {
            std::cout
                << "TEST 8 PASSED: Malformed cancellation rejected safely\n";
        }
        else
        {
            std::cout
                << "TEST 8 FAILED: Malformed cancellation rejected safely\n";

            ++failures;
        }
    }

    if (failures != 0)
    {
        std::cout
            << "\n" << failures
            << " test(s) failed.\n";

        return 1;
    }

    std::cout
        << "\nAll feed-handler tests passed!\n";

    return 0;
}