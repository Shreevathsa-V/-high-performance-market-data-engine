#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "feed/FeedHandler.h"
#include "feed/Protocol.h"
#include "feed/StreamProcessor.h"

namespace
{
    using OrderMessageBuffer =
        std::array<
            std::uint8_t,
            feed::NEW_ORDER_MESSAGE_SIZE>;

    using CancelMessageBuffer =
        std::array<
            std::uint8_t,
            feed::CANCEL_ORDER_MESSAGE_SIZE>;

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

    OrderMessageBuffer CreateOrderMessage(
        std::uint64_t orderId,
        feed::Side side,
        std::int64_t price,
        std::uint64_t quantity)
    {
        OrderMessageBuffer data{};

        std::size_t offset = 0;

        data[offset++] =
            static_cast<std::uint8_t>(
                feed::MessageType::NewOrder);

        WriteBigEndian16(
            data.data() + offset,
            static_cast<std::uint16_t>(
                feed::NEW_ORDER_MESSAGE_SIZE));

        offset += feed::MESSAGE_LENGTH_SIZE;

        WriteBigEndian64(
            data.data() + offset,
            orderId);

        offset += feed::ORDER_ID_SIZE;

        data[offset++] =
            static_cast<std::uint8_t>(side);

        data[offset++] =
            static_cast<std::uint8_t>(
                feed::OrderType::GoodTillCancel);

        WriteBigEndian64(
            data.data() + offset,
            static_cast<std::uint64_t>(price));

        offset += feed::PRICE_SIZE;

        WriteBigEndian64(
            data.data() + offset,
            quantity);

        return data;
    }

    CancelMessageBuffer CreateCancelMessage(
        std::uint64_t orderId)
    {
        CancelMessageBuffer data{};

        data[0] =
            static_cast<std::uint8_t>(
                feed::MessageType::CancelOrder);

        WriteBigEndian16(
            data.data() + feed::MESSAGE_TYPE_SIZE,
            static_cast<std::uint16_t>(
                feed::CANCEL_ORDER_MESSAGE_SIZE));

        WriteBigEndian64(
            data.data() + feed::HEADER_SIZE,
            orderId);

        return data;
    }

    template <typename Buffer>
    void AppendMessage(
        std::vector<std::uint8_t>& destination,
        const Buffer& message)
    {
        destination.insert(
            destination.end(),
            message.begin(),
            message.end());
    }

    void Check(
        const char* description,
        bool condition,
        int& failures)
    {
        if (condition)
        {
            std::cout
                << "PASSED: "
                << description
                << '\n';
        }
        else
        {
            std::cout
                << "FAILED: "
                << description
                << '\n';

            ++failures;
        }
    }
}

int main()
{
    int failures = 0;

    // TEST 1: Process one complete new-order message.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto message = CreateOrderMessage(
            3001,
            feed::Side::Buy,
            10000,
            20);

        const auto result = processor.ProcessBytes(
            message.data(),
            message.size());

        Check(
            "Complete message processed",
            result.Ok() &&
            result.messagesProcessed == 1 &&
            processor.BufferedBytes() == 0 &&
            book.Size() == 1 &&
            book.ValidateBook(),
            failures);
    }

    // TEST 2: An incomplete message remains buffered.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto message = CreateOrderMessage(
            3002,
            feed::Side::Buy,
            10000,
            20);

        const auto result = processor.ProcessBytes(
            message.data(),
            10);

        Check(
            "Partial message buffered without execution",
            result.Ok() &&
            result.messagesProcessed == 0 &&
            processor.BufferedBytes() == 10 &&
            book.Empty() &&
            book.ValidateBook(),
            failures);
    }

    // TEST 3: Remaining bytes complete the buffered message.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto message = CreateOrderMessage(
            3003,
            feed::Side::Buy,
            10000,
            20);

        const auto first = processor.ProcessBytes(
            message.data(),
            10);

        const auto second = processor.ProcessBytes(
            message.data() + 10,
            message.size() - 10);

        Check(
            "Partial message completed by later chunk",
            first.Ok() &&
            first.messagesProcessed == 0 &&
            second.Ok() &&
            second.messagesProcessed == 1 &&
            processor.BufferedBytes() == 0 &&
            book.Size() == 1 &&
            book.ValidateBook(),
            failures);
    }

    // TEST 4: Two complete messages in one input chunk.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto firstMessage = CreateOrderMessage(
            3004,
            feed::Side::Buy,
            10000,
            20);

        const auto secondMessage = CreateOrderMessage(
            3005,
            feed::Side::Buy,
            9900,
            15);

        std::vector<std::uint8_t> combined;

        AppendMessage(combined, firstMessage);
        AppendMessage(combined, secondMessage);

        const auto result = processor.ProcessBytes(
            combined.data(),
            combined.size());

        Check(
            "Multiple messages processed from one chunk",
            result.Ok() &&
            result.messagesProcessed == 2 &&
            processor.BufferedBytes() == 0 &&
            book.Size() == 2 &&
            book.ValidateBook(),
            failures);
    }

    // TEST 5: Submit an order and cancel it in one chunk.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto newOrder = CreateOrderMessage(
            3006,
            feed::Side::Buy,
            10000,
            20);

        const auto cancelMessage =
            CreateCancelMessage(3006);

        std::vector<std::uint8_t> combined;

        AppendMessage(combined, newOrder);
        AppendMessage(combined, cancelMessage);

        const auto result = processor.ProcessBytes(
            combined.data(),
            combined.size());

        Check(
            "Order submission and cancellation in one chunk",
            result.Ok() &&
            result.messagesProcessed == 2 &&
            processor.BufferedBytes() == 0 &&
            book.Empty() &&
            book.ValidateBook(),
            failures);
    }

    // TEST 6: A malformed message is rejected by the handler.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        auto message = CreateOrderMessage(
            3007,
            feed::Side::Buy,
            10000,
            20);

        // Corrupt the declared message length.
        WriteBigEndian16(
            message.data() + feed::MESSAGE_TYPE_SIZE,
            28);

        const auto result = processor.ProcessBytes(
            message.data(),
            message.size());

                Check(
            "Malformed message rejected without adding an order",
            result.Ok() &&
            result.messagesProcessed == 1 &&
            result.messagesRejected == 1 &&
            result.lastFeedError ==
                feed::FeedError::DecodeFailed &&
            result.lastDecodeError ==
                feed::DecodeError::InvalidMessageLength &&
            processor.BufferedBytes() == 0 &&
            book.Empty() &&
            book.ValidateBook(),
            failures);
    }

    // TEST 7: Unsupported message type.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const std::uint8_t invalidType = 99;

        const auto result = processor.ProcessBytes(
            &invalidType,
            1);

        Check(
            "Unsupported message type reported",
            result.error ==
                feed::StreamError::UnsupportedMessageType &&
            processor.BufferedBytes() == 0 &&
            book.Empty(),
            failures);
    }

    // TEST 8: Reset clears a partial message.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto message = CreateOrderMessage(
            3008,
            feed::Side::Buy,
            10000,
            20);

        processor.ProcessBytes(
            message.data(),
            10);

        processor.Reset();

        Check(
            "Reset clears buffered bytes",
            processor.BufferedBytes() == 0 &&
            book.Empty() &&
            book.ValidateBook(),
            failures);
    }

    // TEST 9: Finishing a stream with no incomplete message.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto message = CreateOrderMessage(
            3009,
            feed::Side::Buy,
            10000,
            20);

        const auto processResult = processor.ProcessBytes(
            message.data(),
            message.size());

        const auto finishResult = processor.Finish();

        Check(
            "Finish succeeds after a complete message",
            processResult.Ok() &&
            processResult.messagesProcessed == 1 &&
            finishResult.Ok() &&
            finishResult.bufferedBytes == 0 &&
            processor.BufferedBytes() == 0 &&
            book.Size() == 1 &&
            book.ValidateBook(),
            failures);
    }

    // TEST 10: Finishing a stream with an incomplete message.
    {
        OrderBook book(false);
        feed::FeedHandler handler(book);
        feed::StreamProcessor processor(handler);

        const auto message = CreateOrderMessage(
            3010,
            feed::Side::Buy,
            10000,
            20);

        const auto processResult = processor.ProcessBytes(
            message.data(),
            10);

        const auto finishResult = processor.Finish();

        Check(
            "Finish detects an incomplete message",
            processResult.Ok() &&
            processResult.messagesProcessed == 0 &&
            finishResult.error ==
                feed::StreamError::IncompleteMessage &&
            finishResult.bufferedBytes == 10 &&
            processor.BufferedBytes() == 0 &&
            book.Empty() &&
            book.ValidateBook(),
            failures);
    }

    if (failures != 0)
    {
        std::cout
            << "\n"
            << failures
            << " stream processor test(s) failed.\n";

        return 1;
    }

    std::cout
        << "\nAll stream processor tests passed!\n";

    return 0;
}