#include "feed/FeedHandler.h"

#include <memory>
#include <utility>

#include "Order.h"
#include "OrderType.h"
#include "Side.h"

namespace feed
{
    FeedHandler::FeedHandler(OrderBook& orderBook) noexcept
        : orderBook_(orderBook)
    {
    }

    FeedResult FeedHandler::ProcessMessage(
        const std::uint8_t* data,
        std::size_t size)
    {
        // Validate that a complete message header exists.
        if (data == nullptr)
        {
            return {
                FeedError::DecodeFailed,
                DecodeError::NullBuffer
            };
        }

        if (size < HEADER_SIZE)
        {
            return {
                FeedError::DecodeFailed,
                DecodeError::IncompleteHeader
            };
        }

        // Identify the message type.
        const auto messageType =
            static_cast<MessageType>(data[0]);

        // -------------------------------------------------
        // CASE 1: New order
        // -------------------------------------------------
        if (messageType == MessageType::NewOrder)
        {
            const DecodeResult decoded =
                DecodeNewOrder(data, size);

            if (!decoded.Ok())
            {
                return {
                    FeedError::DecodeFailed,
                    decoded.error
                };
            }

            const ::Side engineSide =
                decoded.message.side == feed::Side::Buy
                    ? ::Side::Buy
                    : ::Side::Sell;

            ::OrderType engineOrderType =
                ::OrderType::GoodTillCancel;

            switch (decoded.message.orderType)
            {
                case feed::OrderType::GoodTillCancel:
                    engineOrderType =
                        ::OrderType::GoodTillCancel;
                    break;

                case feed::OrderType::FillAndKill:
                    engineOrderType =
                        ::OrderType::FillAndKill;
                    break;

                case feed::OrderType::FillOrKill:
                    engineOrderType =
                        ::OrderType::FillOrKill;
                    break;

                case feed::OrderType::GoodForDay:
                    engineOrderType =
                        ::OrderType::GoodForDay;
                    break;

                case feed::OrderType::Market:
                    engineOrderType =
                        ::OrderType::Market;
                    break;
            }

            auto order = std::make_shared<Order>(
                engineOrderType,
                static_cast<OrderId>(
                    decoded.message.orderId),
                engineSide,
                static_cast<Price>(
                    decoded.message.price),
                static_cast<Quantity>(
                    decoded.message.quantity));

            OrderResult result =
                orderBook_.AddOrder(order);

            FeedResult feedResult;
            feedResult.orderResult = std::move(result);

            return feedResult;
        }

        // -------------------------------------------------
        // CASE 2: Cancel order
        // -------------------------------------------------
        if (messageType == MessageType::CancelOrder)
        {
            DecodeError decodeError =
                DecodeError::None;

            const CancelOrderMessage decoded =
                DecodeCancelOrder(
                    data,
                    size,
                    decodeError);

            if (decodeError != DecodeError::None)
            {
                return {
                    FeedError::DecodeFailed,
                    decodeError
                };
            }

            const bool cancelled =
                orderBook_.CancelOrder(
                    static_cast<OrderId>(
                        decoded.orderId));

            FeedResult result;
            result.cancellationProcessed = true;
            result.orderCancelled = cancelled;

            return result;
        }

        // -------------------------------------------------
        // CASE 3: Unsupported message type
        // -------------------------------------------------
        return {
            FeedError::DecodeFailed,
            DecodeError::UnsupportedMessageType
        };
    }
}