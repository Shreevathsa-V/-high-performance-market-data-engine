#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "feed/FeedHandler.h"

namespace feed
{
    enum class StreamError
    {
        None,
        NullBuffer,
        UnsupportedMessageType,
        IncompleteMessage
    };

    struct StreamResult
    {
        StreamError error{StreamError::None};

        // Number of complete messages submitted to the handler.
        std::size_t messagesProcessed{0};

        // Number of messages rejected by the feed handler.
        std::size_t messagesRejected{0};

        // Number of bytes still waiting for a complete message.
        std::size_t bufferedBytes{0};

        // Error information for the most recently rejected message.
        FeedError lastFeedError{FeedError::None};
        DecodeError lastDecodeError{DecodeError::None};

        bool Ok() const noexcept
        {
            return error == StreamError::None;
        }
    };

    class StreamProcessor
    {
    public:
        explicit StreamProcessor(FeedHandler& handler) noexcept;

        StreamResult ProcessBytes(
            const std::uint8_t* data,
            std::size_t size);
        
        StreamResult Finish() noexcept;
        
        std::size_t BufferedBytes() const noexcept;

        void Reset() noexcept;

    private:
        FeedHandler& handler_;

        std::vector<std::uint8_t> buffer_;

        bool GetExpectedMessageSize(
            std::size_t& messageSize) const noexcept;
    };
}