#include "feed/StreamProcessor.h"

#include <algorithm>

#include "feed/Protocol.h"

namespace feed
{
    StreamProcessor::StreamProcessor(
        FeedHandler& handler) noexcept
        : handler_(handler)
    {
    }

    bool StreamProcessor::GetExpectedMessageSize(
        std::size_t& messageSize) const noexcept
    {
        if (buffer_.empty())
        {
            return false;
        }

        const auto messageType =
            static_cast<MessageType>(buffer_[0]);

        switch (messageType)
        {
            case MessageType::NewOrder:
                messageSize = NEW_ORDER_MESSAGE_SIZE;
                return true;

            case MessageType::CancelOrder:
                messageSize = CANCEL_ORDER_MESSAGE_SIZE;
                return true;
        }

        return false;
    }

    StreamResult StreamProcessor::ProcessBytes(
        const std::uint8_t* data,
        std::size_t size)
    {
        StreamResult result;

        if (data == nullptr && size > 0)
        {
            result.error = StreamError::NullBuffer;
            result.bufferedBytes = buffer_.size();
            return result;
        }

        if (size == 0)
        {
            result.bufferedBytes = buffer_.size();
            return result;
        }

        std::size_t offset = 0;

        // -------------------------------------------------
        // PATH 1: Complete a message buffered by an earlier
        // TCP read.
        // -------------------------------------------------

        if (!buffer_.empty())
        {
            std::size_t expectedSize = 0;

            if (!GetExpectedMessageSize(expectedSize))
            {
                buffer_.clear();

                result.error =
                    StreamError::UnsupportedMessageType;

                result.bufferedBytes = 0;
                return result;
            }

            const std::size_t missingBytes =
                expectedSize - buffer_.size();

            const std::size_t availableBytes =
                size;

            const std::size_t bytesToCopy =
                std::min(missingBytes, availableBytes);

            buffer_.insert(
                buffer_.end(),
                data,
                data + bytesToCopy);

            offset += bytesToCopy;

            // The message is still incomplete.
            if (buffer_.size() < expectedSize)
            {
                result.bufferedBytes = buffer_.size();
                return result;
            }

            // The buffered message is now complete.
            const FeedResult feedResult =
                handler_.ProcessMessage(
                    buffer_.data(),
                    expectedSize);

            ++result.messagesProcessed;

            if (!feedResult.Ok())
            {
                ++result.messagesRejected;

                result.lastFeedError =
                    feedResult.error;

                result.lastDecodeError =
                    feedResult.decodeError;
            }

            buffer_.clear();
        }

        // -------------------------------------------------
        // PATH 2: Process complete messages directly from
        // the incoming data without copying them into
        // buffer_.
        // -------------------------------------------------

        while (offset < size)
        {
            const auto messageType =
                static_cast<MessageType>(data[offset]);

            std::size_t expectedSize = 0;

            switch (messageType)
            {
                case MessageType::NewOrder:
                    expectedSize =
                        NEW_ORDER_MESSAGE_SIZE;
                    break;

                case MessageType::CancelOrder:
                    expectedSize =
                        CANCEL_ORDER_MESSAGE_SIZE;
                    break;

                default:
                    result.error =
                        StreamError::UnsupportedMessageType;

                    result.bufferedBytes = 0;
                    return result;
            }

            const std::size_t remainingBytes =
                size - offset;

            // If the message is incomplete, buffer only
            // its remaining bytes for the next TCP read.
            if (remainingBytes < expectedSize)
            {
                buffer_.insert(
                    buffer_.end(),
                    data + offset,
                    data + size);

                result.bufferedBytes =
                    buffer_.size();

                return result;
            }

            // A complete message is already available.
            // Pass it directly to the feed handler.
            const FeedResult feedResult =
                handler_.ProcessMessage(
                    data + offset,
                    expectedSize);

            ++result.messagesProcessed;

            if (!feedResult.Ok())
            {
                ++result.messagesRejected;

                result.lastFeedError =
                    feedResult.error;

                result.lastDecodeError =
                    feedResult.decodeError;
            }

            offset += expectedSize;
        }

        result.bufferedBytes = buffer_.size();

        return result;
    }

    StreamResult StreamProcessor::Finish() noexcept
    {
        StreamResult result{};

        if (!buffer_.empty())
        {
            result.error =
                StreamError::IncompleteMessage;

            result.bufferedBytes =
                buffer_.size();

            buffer_.clear();

            return result;
        }

        result.bufferedBytes = 0;

        return result;
    }

    std::size_t StreamProcessor::BufferedBytes() const noexcept
    {
        return buffer_.size();
    }

    void StreamProcessor::Reset() noexcept
    {
        buffer_.clear();
    }
}