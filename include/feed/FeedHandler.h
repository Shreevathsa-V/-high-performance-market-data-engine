#pragma once

#include <cstddef>
#include <cstdint>

#include "feed/Decoder.h"
#include "OrderBook.h"
#include "OrderResult.h"

namespace feed
{
    enum class FeedError
    {
        None,
        DecodeFailed,
        UnsupportedMessageType
    };

    struct FeedResult
    {
        FeedError error{FeedError::None};
        DecodeError decodeError{DecodeError::None};

        // Populated when a new order is processed.
        OrderResult orderResult{};

        // Populated when a cancellation is processed.
        bool cancellationProcessed{false};
        bool orderCancelled{false};

        bool Ok() const noexcept
        {
            return error == FeedError::None;
        }
    };

    class FeedHandler
    {
    public:
        explicit FeedHandler(OrderBook& orderBook) noexcept;

        FeedResult ProcessMessage(
            const std::uint8_t* data,
            std::size_t size);

    private:
        OrderBook& orderBook_;
    };
}