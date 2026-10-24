#pragma once

#include <cstddef>
#include <cstdint>

namespace feed
{
    // Message types supported by our simulated protocol.
    enum class MessageType : std::uint8_t
    {
        NewOrder = 1,
        CancelOrder = 2
    };

    // Order side encoded in the protocol.
    enum class Side : std::uint8_t
    {
        Buy = 0,
        Sell = 1
    };

    // Order types supported by the protocol.
    enum class OrderType : std::uint8_t
    {
        GoodTillCancel = 0,
        FillAndKill = 1,
        FillOrKill = 2,
        GoodForDay = 3,
        Market = 4
    };

    // Wire-format constants.
    //
    // All multibyte integer fields will use network byte order
    // (big-endian). We will implement explicit encoding and
    // decoding rather than transmitting a C++ struct directly.

    constexpr std::size_t MESSAGE_TYPE_SIZE = 1;
    constexpr std::size_t MESSAGE_LENGTH_SIZE = 2;

    constexpr std::size_t ORDER_ID_SIZE = 8;
    constexpr std::size_t SIDE_SIZE = 1;
    constexpr std::size_t ORDER_TYPE_SIZE = 1;
    constexpr std::size_t PRICE_SIZE = 8;
    constexpr std::size_t QUANTITY_SIZE = 8;

    constexpr std::size_t HEADER_SIZE = MESSAGE_TYPE_SIZE + MESSAGE_LENGTH_SIZE;

    constexpr std::size_t NEW_ORDER_MESSAGE_SIZE =
        HEADER_SIZE +
        ORDER_ID_SIZE +
        SIDE_SIZE +
        ORDER_TYPE_SIZE +
        PRICE_SIZE +
        QUANTITY_SIZE;

    constexpr std::size_t CANCEL_ORDER_MESSAGE_SIZE = HEADER_SIZE + ORDER_ID_SIZE;

    static_assert(HEADER_SIZE == 3);
    static_assert(NEW_ORDER_MESSAGE_SIZE == 29);

    static_assert(CANCEL_ORDER_MESSAGE_SIZE == 11);
    static_assert(CANCEL_ORDER_MESSAGE_SIZE < NEW_ORDER_MESSAGE_SIZE);
}