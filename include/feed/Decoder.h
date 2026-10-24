#pragma once

#include <cstddef>
#include <cstdint>

#include "Protocol.h"

namespace feed
{
    // Represents a successfully decoded new-order message.
    struct NewOrderMessage
    {
        std::uint64_t orderId;
        Side side;
        OrderType orderType;
        std::int64_t price;
        std::uint64_t quantity;
    };

    // Represents a successfully decoded cancellation message.
    struct CancelOrderMessage
    {
        std::uint64_t orderId;
    };

    // Errors that may occur while decoding a message.
    enum class DecodeError
    {
        None,
        NullBuffer,
        IncompleteHeader,
        UnsupportedMessageType,
        InvalidMessageLength,
        IncompleteMessage,
        TrailingBytes,
        InvalidOrderId,
        InvalidSide,
        InvalidOrderType,
        InvalidPrice,
        InvalidQuantity
    };

    // Contains either a decoded message or an error.
    struct DecodeResult
    {
        DecodeError error{DecodeError::None};
        NewOrderMessage message{};

        bool Ok() const noexcept
        {
            return error == DecodeError::None;
        }
    };

    // Decode exactly one complete new-order message.
    //
    // Multibyte integer fields use big-endian byte order.
    // The input buffer is not modified.
    DecodeResult DecodeNewOrder(const std::uint8_t* data, std::size_t size) noexcept;

    // Decode exactly one complete cancellation message.
    CancelOrderMessage DecodeCancelOrder(const std::uint8_t* data, std::size_t size, DecodeError& error) noexcept;

    // Convert a decoding error into a readable string.
    const char* DecodeErrorMessage(DecodeError error) noexcept;
}