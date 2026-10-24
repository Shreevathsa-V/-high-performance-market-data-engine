#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "feed/Decoder.h"
#include "feed/Protocol.h"

namespace
{
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

    bool Check(
        const char* description,
        bool condition,
        int& failures)
    {
        if (condition)
        {
            std::cout << "PASSED: " << description << '\n';
            return true;
        }

        std::cout << "FAILED: " << description << '\n';
        ++failures;

        return false;
    }
}

int main()
{
    int failures = 0;

    // TEST 1: A valid cancellation message.
    {
        const auto data = CreateCancelMessage(1001);

        feed::DecodeError error = feed::DecodeError::None;

        const auto decoded = feed::DecodeCancelOrder(
            data.data(),
            data.size(),
            error);

        Check(
            "Valid cancellation decoded correctly",
            error == feed::DecodeError::None &&
            decoded.orderId == 1001,
            failures);
    }

    // TEST 2: Null input.
    {
        feed::DecodeError error = feed::DecodeError::None;

        feed::DecodeCancelOrder(
            nullptr,
            feed::CANCEL_ORDER_MESSAGE_SIZE,
            error);

        Check(
            "Null buffer rejected",
            error == feed::DecodeError::NullBuffer,
            failures);
    }

    // TEST 3: Incomplete header.
    {
        const auto data = CreateCancelMessage(1001);

        feed::DecodeError error = feed::DecodeError::None;

        feed::DecodeCancelOrder(
            data.data(),
            2,
            error);

        Check(
            "Incomplete header rejected",
            error == feed::DecodeError::IncompleteHeader,
            failures);
    }

    // TEST 4: Wrong message type.
    {
        auto data = CreateCancelMessage(1001);

        data[0] =
            static_cast<std::uint8_t>(
                feed::MessageType::NewOrder);

        feed::DecodeError error = feed::DecodeError::None;

        feed::DecodeCancelOrder(
            data.data(),
            data.size(),
            error);

        Check(
            "Wrong message type rejected",
            error == feed::DecodeError::UnsupportedMessageType,
            failures);
    }

    // TEST 5: Incorrect message length.
    {
        auto data = CreateCancelMessage(1001);

        WriteBigEndian16(
            data.data() + feed::MESSAGE_TYPE_SIZE,
            10);

        feed::DecodeError error = feed::DecodeError::None;

        feed::DecodeCancelOrder(
            data.data(),
            data.size(),
            error);

        Check(
            "Incorrect message length rejected",
            error == feed::DecodeError::InvalidMessageLength,
            failures);
    }

    // TEST 6: Truncated message.
    {
        const auto data = CreateCancelMessage(1001);

        feed::DecodeError error = feed::DecodeError::None;

        feed::DecodeCancelOrder(
            data.data(),
            data.size() - 1,
            error);

        Check(
            "Truncated cancellation rejected",
            error == feed::DecodeError::IncompleteMessage,
            failures);
    }

    // TEST 7: Trailing bytes.
    {
        const auto data = CreateCancelMessage(1001);

        std::vector<std::uint8_t> extended(
            data.begin(),
            data.end());

        extended.push_back(0);

        feed::DecodeError error = feed::DecodeError::None;

        feed::DecodeCancelOrder(
            extended.data(),
            extended.size(),
            error);

        Check(
            "Trailing bytes rejected",
            error == feed::DecodeError::TrailingBytes,
            failures);
    }

    // TEST 8: Zero order ID.
    {
        const auto data = CreateCancelMessage(0);

        feed::DecodeError error = feed::DecodeError::None;

        feed::DecodeCancelOrder(
            data.data(),
            data.size(),
            error);

        Check(
            "Zero order ID rejected",
            error == feed::DecodeError::InvalidOrderId,
            failures);
    }

    if (failures != 0)
    {
        std::cout
            << "\n"
            << failures
            << " cancellation decoder test(s) failed.\n";

        return 1;
    }

    std::cout
        << "\nAll cancellation decoder tests passed!\n";

    return 0;
}