#include "feed/Decoder.h"

#include <bit>
#include <limits>

namespace feed
{
    namespace
    {
        // Read a 16-bit unsigned integer in big-endian order.
        std::uint16_t ReadBigEndian16(
            const std::uint8_t* data) noexcept
        {
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(data[0]) << 8) |
                static_cast<std::uint16_t>(data[1]));
        }

        // Read a 64-bit unsigned integer in big-endian order.
        std::uint64_t ReadBigEndian64(
            const std::uint8_t* data) noexcept
        {
            std::uint64_t value = 0;

            for (std::size_t i = 0; i < 8; ++i)
            {
                value = (value << 8) |
                        static_cast<std::uint64_t>(data[i]);
            }

            return value;
        }
    }

    DecodeResult DecodeNewOrder(
        const std::uint8_t* data,
        std::size_t size) noexcept
    {
        // -------------------------------------------------
        // 1. Validate the input buffer.
        // -------------------------------------------------

        if (data == nullptr)
        {
            return {DecodeError::NullBuffer, {}};
        }

        if (size < HEADER_SIZE)
        {
            return {DecodeError::IncompleteHeader, {}};
        }

        // -------------------------------------------------
        // 2. Decode the message type.
        // -------------------------------------------------

        if (data[0] !=
            static_cast<std::uint8_t>(MessageType::NewOrder))
        {
            return {
                DecodeError::UnsupportedMessageType,
                {}
            };
        }

        // -------------------------------------------------
        // 3. Decode and validate the message length.
        // -------------------------------------------------

        const std::uint16_t messageLength =
            ReadBigEndian16(data + MESSAGE_TYPE_SIZE);

        if (messageLength != NEW_ORDER_MESSAGE_SIZE)
        {
            return {
                DecodeError::InvalidMessageLength,
                {}
            };
        }

        if (size < messageLength)
        {
            return {
                DecodeError::IncompleteMessage,
                {}
            };
        }

        if (size > messageLength)
        {
            return {
                DecodeError::TrailingBytes,
                {}
            };
        }

        // -------------------------------------------------
        // 4. Decode the order ID.
        // -------------------------------------------------

        std::size_t offset = HEADER_SIZE;

        const std::uint64_t orderId =
            ReadBigEndian64(data + offset);

        offset += ORDER_ID_SIZE;

        if (orderId == 0)
        {
            return {
                DecodeError::InvalidOrderId,
                {}
            };
        }

        // -------------------------------------------------
        // 5. Decode and validate the side.
        // -------------------------------------------------

        const std::uint8_t rawSide = data[offset];

        ++offset;

        if (rawSide >
            static_cast<std::uint8_t>(Side::Sell))
        {
            return {
                DecodeError::InvalidSide,
                {}
            };
        }

        const Side side = static_cast<Side>(rawSide);

        // -------------------------------------------------
        // 6. Decode and validate the order type.
        // -------------------------------------------------

        const std::uint8_t rawOrderType = data[offset];

        ++offset;

        if (rawOrderType >
            static_cast<std::uint8_t>(OrderType::Market))
        {
            return {
                DecodeError::InvalidOrderType,
                {}
            };
        }

        const OrderType orderType =
            static_cast<OrderType>(rawOrderType);

        // -------------------------------------------------
        // 7. Decode and validate the price.
        // -------------------------------------------------

        const std::uint64_t rawPrice =
            ReadBigEndian64(data + offset);

        ++offset;
        offset += PRICE_SIZE - 1;

        static_assert(
            sizeof(std::uint64_t) == sizeof(std::int64_t));

        const std::int64_t price =
            std::bit_cast<std::int64_t>(rawPrice);

        if (price <= 0)
        {
            return {
                DecodeError::InvalidPrice,
                {}
            };
        }

        // -------------------------------------------------
        // 8. Decode and validate the quantity.
        // -------------------------------------------------

        const std::uint64_t quantity =
            ReadBigEndian64(data + offset);

        if (quantity == 0)
        {
            return {
                DecodeError::InvalidQuantity,
                {}
            };
        }

        // -------------------------------------------------
        // 9. Return the successfully decoded message.
        // -------------------------------------------------

        return {
            DecodeError::None,
            NewOrderMessage{
                orderId,
                side,
                orderType,
                price,
                quantity
            }
        };
    }

    CancelOrderMessage DecodeCancelOrder(
        const std::uint8_t* data,
        std::size_t size,
        DecodeError& error) noexcept
    {
        // 1. Validate the input buffer.
        if (data == nullptr)
        {
            error = DecodeError::NullBuffer;
            return {};
        }

        // 2. Validate the header.
        if (size < HEADER_SIZE)
        {
            error = DecodeError::IncompleteHeader;
            return {};
        }

        // 3. Validate the message type.
        if (data[0] !=
            static_cast<std::uint8_t>(MessageType::CancelOrder))
        {
            error = DecodeError::UnsupportedMessageType;
            return {};
        }

        // 4. Decode and validate the message length.
        const std::uint16_t messageLength =
            ReadBigEndian16(data + MESSAGE_TYPE_SIZE);

        if (messageLength != CANCEL_ORDER_MESSAGE_SIZE)
        {
            error = DecodeError::InvalidMessageLength;
            return {};
        }

        // 5. Reject incomplete messages.
        if (size < messageLength)
        {
            error = DecodeError::IncompleteMessage;
            return {};
        }

        // 6. Reject extra bytes.
        if (size > messageLength)
        {
            error = DecodeError::TrailingBytes;
            return {};
        }

        // 7. Decode the order ID.
        const std::uint64_t orderId =
            ReadBigEndian64(data + HEADER_SIZE);

        if (orderId == 0)
        {
            error = DecodeError::InvalidOrderId;
            return {};
        }

        // 8. Return the successfully decoded message.
        error = DecodeError::None;

        return CancelOrderMessage{orderId};
    }

    const char* DecodeErrorMessage(
        DecodeError error) noexcept
    {
        switch (error)
        {
            case DecodeError::None:
                return "Success";

            case DecodeError::NullBuffer:
                return "Input buffer is null";

            case DecodeError::IncompleteHeader:
                return "Incomplete message header";

            case DecodeError::UnsupportedMessageType:
                return "Unsupported message type";

            case DecodeError::InvalidMessageLength:
                return "Invalid message length";

            case DecodeError::IncompleteMessage:
                return "Incomplete message";

            case DecodeError::TrailingBytes:
                return "Unexpected trailing bytes";

            case DecodeError::InvalidOrderId:
                return "Invalid order ID";

            case DecodeError::InvalidSide:
                return "Invalid order side";

            case DecodeError::InvalidOrderType:
                return "Invalid order type";

            case DecodeError::InvalidPrice:
                return "Invalid order price";

            case DecodeError::InvalidQuantity:
                return "Invalid order quantity";
        }

        return "Unknown decoding error";
    }
}