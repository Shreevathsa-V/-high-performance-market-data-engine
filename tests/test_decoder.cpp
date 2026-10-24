#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <vector>

#include "feed/Decoder.h"

namespace
{
    using MessageBuffer =
        std::array<std::uint8_t, feed::NEW_ORDER_MESSAGE_SIZE>;

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

    MessageBuffer CreateValidMessage()
    {
        MessageBuffer data{};

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
            1001);

        offset += feed::ORDER_ID_SIZE;

        data[offset++] =
            static_cast<std::uint8_t>(
                feed::Side::Buy);

        data[offset++] =
            static_cast<std::uint8_t>(
                feed::OrderType::GoodTillCancel);

        WriteBigEndian64(
            data.data() + offset,
            std::bit_cast<std::uint64_t>(
                std::int64_t{10000}));

        offset += feed::PRICE_SIZE;

        WriteBigEndian64(
            data.data() + offset,
            50);

        return data;
    }
}

int main()
{
    int failures = 0;

    // -----------------------------------------------------
    // TEST 1: Decode a valid new-order message.
    // -----------------------------------------------------

    {
        const auto data = CreateValidMessage();

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.Ok() &&
            result.message.orderId == 1001 &&
            result.message.side == feed::Side::Buy &&
            result.message.orderType ==
                feed::OrderType::GoodTillCancel &&
            result.message.price == 10000 &&
            result.message.quantity == 50;

        if (passed)
        {
            std::cout
                << "TEST 1 PASSED: Valid message decoding\n";
        }
        else
        {
            std::cout
                << "TEST 1 FAILED: Valid message decoding\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 2: Reject a null buffer.
    // -----------------------------------------------------

    {
        const auto result =
            feed::DecodeNewOrder(nullptr, 0);

        const bool passed =
            result.error == feed::DecodeError::NullBuffer;

        if (passed)
        {
            std::cout
                << "TEST 2 PASSED: Null buffer rejected\n";
        }
        else
        {
            std::cout
                << "TEST 2 FAILED: Null buffer rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 3: Reject an incomplete header.
    // -----------------------------------------------------

    {
        const std::array<std::uint8_t, 2> data{};

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.error ==
                feed::DecodeError::IncompleteHeader;

        if (passed)
        {
            std::cout
                << "TEST 3 PASSED: Incomplete header rejected\n";
        }
        else
        {
            std::cout
                << "TEST 3 FAILED: Incomplete header rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 4: Reject an unsupported message type.
    // -----------------------------------------------------

    {
        auto data = CreateValidMessage();

        data[0] = 99;

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.error ==
                feed::DecodeError::UnsupportedMessageType;

        if (passed)
        {
            std::cout
                << "TEST 4 PASSED: Unsupported type rejected\n";
        }
        else
        {
            std::cout
                << "TEST 4 FAILED: Unsupported type rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 5: Reject an invalid message length.
    // -----------------------------------------------------

    {
        auto data = CreateValidMessage();

        WriteBigEndian16(
            data.data() + feed::MESSAGE_TYPE_SIZE,
            28);

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.error ==
                feed::DecodeError::InvalidMessageLength;

        if (passed)
        {
            std::cout
                << "TEST 5 PASSED: Invalid length rejected\n";
        }
        else
        {
            std::cout
                << "TEST 5 FAILED: Invalid length rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 6: Reject a truncated message.
    // -----------------------------------------------------

    {
        const auto data = CreateValidMessage();

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size() - 1);

        const bool passed =
            result.error ==
                feed::DecodeError::IncompleteMessage;

        if (passed)
        {
            std::cout
                << "TEST 6 PASSED: Truncated message rejected\n";
        }
        else
        {
            std::cout
                << "TEST 6 FAILED: Truncated message rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 7: Reject an invalid side.
    // -----------------------------------------------------

    {
        auto data = CreateValidMessage();

        const std::size_t sideOffset =
            feed::HEADER_SIZE + feed::ORDER_ID_SIZE;

        data[sideOffset] = 2;

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.error == feed::DecodeError::InvalidSide;

        if (passed)
        {
            std::cout
                << "TEST 7 PASSED: Invalid side rejected\n";
        }
        else
        {
            std::cout
                << "TEST 7 FAILED: Invalid side rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 8: Reject a zero order ID.
    // -----------------------------------------------------

    {
        auto data = CreateValidMessage();

        WriteBigEndian64(
            data.data() + feed::HEADER_SIZE,
            0);

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.error ==
                feed::DecodeError::InvalidOrderId;

        if (passed)
        {
            std::cout
                << "TEST 8 PASSED: Zero order ID rejected\n";
        }
        else
        {
            std::cout
                << "TEST 8 FAILED: Zero order ID rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 9: Reject a zero quantity.
    // -----------------------------------------------------

    {
        auto data = CreateValidMessage();

        const std::size_t quantityOffset =
            feed::HEADER_SIZE +
            feed::ORDER_ID_SIZE +
            feed::SIDE_SIZE +
            feed::ORDER_TYPE_SIZE +
            feed::PRICE_SIZE;

        WriteBigEndian64(
            data.data() + quantityOffset,
            0);

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.error ==
                feed::DecodeError::InvalidQuantity;

        if (passed)
        {
            std::cout
                << "TEST 9 PASSED: Zero quantity rejected\n";
        }
        else
        {
            std::cout
                << "TEST 9 FAILED: Zero quantity rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 10: Reject an invalid price.
    // -----------------------------------------------------

    {
        auto data = CreateValidMessage();

        const std::size_t priceOffset =
            feed::HEADER_SIZE +
            feed::ORDER_ID_SIZE +
            feed::SIDE_SIZE +
            feed::ORDER_TYPE_SIZE;

        WriteBigEndian64(
            data.data() + priceOffset,
            std::bit_cast<std::uint64_t>(
                std::int64_t{0}));

        const auto result =
            feed::DecodeNewOrder(
                data.data(),
                data.size());

        const bool passed =
            result.error == feed::DecodeError::InvalidPrice;

        if (passed)
        {
            std::cout
                << "TEST 10 PASSED: Invalid price rejected\n";
        }
        else
        {
            std::cout
                << "TEST 10 FAILED: Invalid price rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // TEST 11: Reject unexpected trailing bytes.
    // -----------------------------------------------------

    {
        auto data = CreateValidMessage();

        std::vector<std::uint8_t> extended(
            data.begin(),
            data.end());

        extended.push_back(0);

        const auto result =
            feed::DecodeNewOrder(
                extended.data(),
                extended.size());

        const bool passed =
            result.error == feed::DecodeError::TrailingBytes;

        if (passed)
        {
            std::cout
                << "TEST 11 PASSED: Trailing bytes rejected\n";
        }
        else
        {
            std::cout
                << "TEST 11 FAILED: Trailing bytes rejected\n";

            ++failures;
        }
    }

    // -----------------------------------------------------
    // Final result.
    // -----------------------------------------------------

    if (failures != 0)
    {
        std::cout
            << "\n" << failures
            << " test(s) failed.\n";

        return 1;
    }

    std::cout
        << "\nAll decoder tests passed!\n";

    return 0;
}