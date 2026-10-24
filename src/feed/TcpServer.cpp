#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <chrono>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "feed/FeedHandler.h"
#include "feed/StreamProcessor.h"

namespace
{
    constexpr std::uint16_t SERVER_PORT = 9000;
    constexpr int LISTEN_BACKLOG = 8;
    constexpr std::size_t RECEIVE_BUFFER_SIZE = 4096;

    void HandleClient(
        int clientSocket,
        OrderBook& orderBook,
        std::uint64_t clientNumber,
        bool quiet)
    {
        feed::FeedHandler handler(orderBook);
        feed::StreamProcessor processor(handler);

        const auto startTime = std::chrono::steady_clock::now();

        std::uint64_t totalBytesReceived = 0;
        std::uint64_t totalMessagesProcessed = 0;
        std::uint64_t totalMessagesRejected = 0;

        std::array<std::uint8_t, RECEIVE_BUFFER_SIZE> receiveBuffer{};

        if (!quiet)
        {
            std::cout
                << "[SERVER] Client "
                << clientNumber
                << " connected\n";
        }

        while (true)
        {
            const ssize_t bytesReceived = recv(
                clientSocket,
                receiveBuffer.data(),
                receiveBuffer.size(),
                0);

            // A return value of 0 means the client has finished sending.
            if (bytesReceived == 0)
            {
                if (!quiet)
                {
                    std::cout
                        << "[SERVER] Client finished sending data.\n";
                }

                break;
            }

            // Handle receive errors.
            if (bytesReceived < 0)
            {
                // recv() may be interrupted by a signal.
                if (errno == EINTR)
                {
                    continue;
                }

                std::cerr
                    << "[ERROR] recv() failed for client "
                    << clientNumber
                    << ": "
                    << std::strerror(errno)
                    << '\n';

                break;
            }

            const auto result = processor.ProcessBytes(
                receiveBuffer.data(),
                static_cast<std::size_t>(bytesReceived));

            totalBytesReceived +=
                static_cast<std::uint64_t>(bytesReceived);

            totalMessagesProcessed +=
                static_cast<std::uint64_t>(result.messagesProcessed);

            totalMessagesRejected +=
                static_cast<std::uint64_t>(result.messagesRejected);

            // Informational output is disabled in quiet mode.
            if (!quiet)
            {
                std::cout
                    << "[SERVER] Received "
                    << bytesReceived
                    << " bytes; processed "
                    << result.messagesProcessed
                    << " message(s); buffered "
                    << result.bufferedBytes
                    << " byte(s)\n";

                std::cout
                    << "[SERVER] Current order book size: "
                    << orderBook.Size()
                    << '\n';
            }

            // Rejected messages remain visible, even in quiet mode.
            if (result.messagesRejected > 0)
            {
                std::cerr
                    << "[REJECTED] "
                    << result.messagesRejected
                    << " message(s); last feed error: "
                    << static_cast<int>(result.lastFeedError)
                    << "; last decode error: "
                    << static_cast<int>(result.lastDecodeError)
                    << '\n';
            }

            // Stop processing if the stream processor reports an error.
            if (!result.Ok())
            {
                std::cerr
                    << "[ERROR] Stream processing failed: "
                    << static_cast<int>(result.error)
                    << '\n';

                break;
            }
        }

        const auto endTime = std::chrono::steady_clock::now();

        const double elapsedSeconds =
            std::chrono::duration<double>(
                endTime - startTime).count();

        const double messagesPerSecond =
            elapsedSeconds > 0.0
                ? static_cast<double>(totalMessagesProcessed) /
                      elapsedSeconds
                : 0.0;

        // Always print the benchmark summary.
        std::cout
            << "[SUMMARY] Bytes received: "
            << totalBytesReceived << '\n'
            << "[SUMMARY] Messages processed: "
            << totalMessagesProcessed << '\n'
            << "[SUMMARY] Messages rejected: "
            << totalMessagesRejected << '\n'
            << "[SUMMARY] Elapsed seconds: "
            << elapsedSeconds << '\n'
            << "[SUMMARY] Processing rate: "
            << messagesPerSecond
            << " messages/second\n";

        const std::size_t bufferedBytes = processor.BufferedBytes();

        if (bufferedBytes > 0 && !quiet)
        {
            std::cout
                << "[SERVER] Incomplete message discarded: "
                << bufferedBytes
                << " buffered byte(s)\n";
        }

        close(clientSocket);

        if (!quiet)
        {
            std::cout
                << "[SERVER] Closed client "
                << clientNumber
                << '\n';
        }
    }
}

int main(int argc, char* argv[])
{
    const bool quiet =
        argc > 1 && std::string(argv[1]) == "--quiet";

    // The matching engine persists while the server runs.
    OrderBook orderBook(false);

    // Create a TCP listening socket.
    const int serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0);

    if (serverSocket < 0)
    {
        std::cerr
            << "[ERROR] socket() failed: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    // Allow the port to be reused after the server exits.
    int reuseAddress = 1;

    if (setsockopt(
            serverSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)) < 0)
    {
        std::cerr
            << "[ERROR] setsockopt() failed: "
            << std::strerror(errno)
            << '\n';

        close(serverSocket);
        return 1;
    }

    // Bind only to localhost for development and testing.
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) < 0)
    {
        std::cerr
            << "[ERROR] bind() failed: "
            << std::strerror(errno)
            << '\n';

        close(serverSocket);
        return 1;
    }

    // Start listening for incoming connections.
    if (listen(serverSocket, LISTEN_BACKLOG) < 0)
    {
        std::cerr
            << "[ERROR] listen() failed: "
            << std::strerror(errno)
            << '\n';

        close(serverSocket);
        return 1;
    }

    std::cout
        << "[SERVER] Listening on 127.0.0.1:"
        << SERVER_PORT
        << '\n';

    std::uint64_t clientNumber = 0;

    // Accept clients sequentially.
    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const int clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength);

        if (clientSocket < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "[ERROR] accept() failed: "
                << std::strerror(errno)
                << '\n';

            continue;
        }

        ++clientNumber;

        HandleClient(
            clientSocket,
            orderBook,
            clientNumber,
            quiet);
    }

    close(serverSocket);

    return 0;
}