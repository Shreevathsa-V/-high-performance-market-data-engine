#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "feed/FeedHandler.h"
#include "feed/StreamProcessor.h"

namespace
{
    constexpr std::uint16_t SERVER_PORT = 9000;
    constexpr int LISTEN_BACKLOG = 128;
    constexpr int MAX_EVENTS = 128;
    constexpr std::size_t RECEIVE_BUFFER_SIZE = 4096;

    using Clock = std::chrono::steady_clock;

    struct ClientState
    {
        feed::FeedHandler handler;
        feed::StreamProcessor processor;

        Clock::time_point startTime{Clock::now()};

        std::uint64_t bytesReceived{0};
        std::uint64_t messagesProcessed{0};
        std::uint64_t messagesRejected{0};

        explicit ClientState(OrderBook& orderBook)
            : handler(orderBook),
              processor(handler)
        {
        }
    };

    void PrintSummary(int clientSocket, const ClientState& client)
    {
        const double elapsedSeconds =
            std::chrono::duration<double>(
                Clock::now() - client.startTime).count();

        const double messageRate =
            elapsedSeconds > 0.0
                ? static_cast<double>(client.messagesProcessed) /
                      elapsedSeconds
                : 0.0;

                std::cout
                    << "[SUMMARY] Client socket: " << clientSocket << '\n'
                    << "[SUMMARY] Bytes received: "
                    << client.bytesReceived << '\n'
                    << "[SUMMARY] Messages processed: "
                    << client.messagesProcessed << '\n'
                    << "[SUMMARY] Messages rejected: "
                    << client.messagesRejected << '\n'
                    << "[SUMMARY] Elapsed seconds: "
                    << elapsedSeconds << '\n'
                    << "[SUMMARY] Processing rate: "
                    << messageRate << " messages/second\n";

                // Machine-readable statistics for benchmark verification.
                std::cout
                    << "BENCHMARK_RESULT {\"socket\":"
                    << clientSocket
                    << ",\"bytes_received\":"
                    << client.bytesReceived
                    << ",\"messages_processed\":"
                    << client.messagesProcessed
                    << ",\"messages_rejected\":"
                    << client.messagesRejected
                    << "}\n";

                std::cout.flush();
    }

    void CloseClient(
        int epollFd,
        int clientSocket,
        std::unordered_map<int, std::unique_ptr<ClientState>>& clients)
    {
        const auto it = clients.find(clientSocket);

        if (it == clients.end())
        {
            return;
        }

        PrintSummary(clientSocket, *it->second);

        epoll_ctl(epollFd, EPOLL_CTL_DEL, clientSocket, nullptr);
        close(clientSocket);

        clients.erase(it);

        std::cout
            << "[SERVER] Client socket "
            << clientSocket
            << " disconnected\n";
    }

    bool AddClient(
        int epollFd,
        int clientSocket,
        OrderBook& orderBook,
        std::unordered_map<int, std::unique_ptr<ClientState>>& clients)
    {
        epoll_event event{};
        #ifdef USE_EDGE_TRIGGERED
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        #else
        event.events = EPOLLIN | EPOLLRDHUP;
        #endif
        event.data.fd = clientSocket;

        if (epoll_ctl(
                epollFd,
                EPOLL_CTL_ADD,
                clientSocket,
                &event) < 0)
        {
            std::cerr
                << "[ERROR] Could not register client socket: "
                << std::strerror(errno) << '\n';

            close(clientSocket);
            return false;
        }

        clients.emplace(
            clientSocket,
            std::make_unique<ClientState>(orderBook));

        std::cout
            << "[SERVER] Client connected; socket "
            << clientSocket << '\n';

        return true;
    }

    void AcceptClients(
        int listenFd,
        int epollFd,
        OrderBook& orderBook,
        std::unordered_map<int, std::unique_ptr<ClientState>>& clients)
    {
        while (true)
        {
            // Linux accept4 creates a nonblocking client socket.
            const int clientSocket = accept4(
                listenFd,
                nullptr,
                nullptr,
                SOCK_NONBLOCK | SOCK_CLOEXEC);

            if (clientSocket < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // All currently pending connections have been accepted.
                    return;
                }

                std::cerr
                    << "[ERROR] accept4() failed: "
                    << std::strerror(errno) << '\n';

                return;
            }

            AddClient(
                epollFd,
                clientSocket,
                orderBook,
                clients);
        }
    }

    bool ProcessClientEvents(
        int epollFd,
        int clientSocket,
        std::uint32_t eventFlags,
        std::unordered_map<int, std::unique_ptr<ClientState>>& clients)
    {
        const auto it = clients.find(clientSocket);

        if (it == clients.end())
        {
            return false;
        }

        ClientState& client = *it->second;

        std::array<std::uint8_t, RECEIVE_BUFFER_SIZE> receiveBuffer{};

        bool shouldClose = false;

        // Drain all currently available data from this nonblocking socket.
        while (true)
        {
            const ssize_t bytesReceived = recv(
                clientSocket,
                receiveBuffer.data(),
                receiveBuffer.size(),
                0);

            if (bytesReceived > 0)
            {
                client.bytesReceived +=
                    static_cast<std::uint64_t>(bytesReceived);

                const feed::StreamResult result =
                    client.processor.ProcessBytes(
                        receiveBuffer.data(),
                        static_cast<std::size_t>(bytesReceived));

                client.messagesProcessed +=
                    static_cast<std::uint64_t>(
                        result.messagesProcessed);

                client.messagesRejected +=
                    static_cast<std::uint64_t>(
                        result.messagesRejected);

                if (result.messagesRejected > 0)
                {
                    std::cerr
                        << "[REJECTED] Socket " << clientSocket
                        << ": " << result.messagesRejected
                        << " message(s) rejected; feed error="
                        << static_cast<int>(result.lastFeedError)
                        << ", decode error="
                        << static_cast<int>(result.lastDecodeError)
                        << '\n';
                }

                if (!result.Ok())
                {
                    std::cerr
                        << "[ERROR] Stream processing failed for socket "
                        << clientSocket
                        << "; stream error="
                        << static_cast<int>(result.error)
                        << '\n';

                    shouldClose = true;
                    break;
                }

                // Continue reading until recv() would block.
                continue;
            }

            if (bytesReceived == 0)
            {
                // The client has closed its sending side.
                shouldClose = true;
                break;
            }

            if (errno == EINTR)
            {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // No more data is available right now.
                break;
            }

            std::cerr
                << "[ERROR] recv() failed for socket "
                << clientSocket << ": "
                << std::strerror(errno) << '\n';

            shouldClose = true;
            break;
        }

        // Handle a half-close or socket error after draining readable data.
        if (eventFlags & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
        {
            shouldClose = true;
        }

        if (shouldClose)
        {
            const auto finishResult = client.processor.Finish();

            if (finishResult.error ==
                feed::StreamError::IncompleteMessage)
            {
                std::cerr
                    << "[WARN] Client " << clientSocket
                    << " disconnected with an incomplete message. "
                    << "Buffered bytes: "
                    << finishResult.bufferedBytes
                    << '\n';
            }

            CloseClient(epollFd, clientSocket, clients);
            return true;
        }

        return false;
    }
}

int main()
{
    // One event loop owns the order book, so no locking is needed here.
    OrderBook orderBook(false);

    // Create a nonblocking listening socket.
    const int listenFd = socket(
        AF_INET,
        SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
        0);

    if (listenFd < 0)
    {
        std::cerr
            << "[ERROR] socket() failed: "
            << std::strerror(errno) << '\n';

        return 1;
    }

    int reuseAddress = 1;

    if (setsockopt(
            listenFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)) < 0)
    {
        std::cerr
            << "[ERROR] setsockopt() failed: "
            << std::strerror(errno) << '\n';

        close(listenFd);
        return 1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;

    // Bind to all container interfaces so Docker can forward port 9000.
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(
            listenFd,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) < 0)
    {
        std::cerr
            << "[ERROR] bind() failed: "
            << std::strerror(errno) << '\n';

        close(listenFd);
        return 1;
    }

    if (listen(listenFd, LISTEN_BACKLOG) < 0)
    {
        std::cerr
            << "[ERROR] listen() failed: "
            << std::strerror(errno) << '\n';

        close(listenFd);
        return 1;
    }

    // Create the Linux epoll instance.
    const int epollFd = epoll_create1(EPOLL_CLOEXEC);

    if (epollFd < 0)
    {
        std::cerr
            << "[ERROR] epoll_create1() failed: "
            << std::strerror(errno) << '\n';

        close(listenFd);
        return 1;
    }

    // Register the listening socket with epoll.
    epoll_event listenEvent{};
    #ifdef USE_EDGE_TRIGGERED

    listenEvent.events = EPOLLIN | EPOLLET;

    #else

    listenEvent.events = EPOLLIN;

    #endif
    listenEvent.data.fd = listenFd;

    if (epoll_ctl(
            epollFd,
            EPOLL_CTL_ADD,
            listenFd,
            &listenEvent) < 0)
    {
        std::cerr
            << "[ERROR] Could not register listening socket: "
            << std::strerror(errno) << '\n';

        close(epollFd);
        close(listenFd);
        return 1;
    }

    std::unordered_map<int, std::unique_ptr<ClientState>> clients;
    std::array<epoll_event, MAX_EVENTS> events{};

    std::cout
        << "[SERVER] Linux epoll server listening on port "
        << SERVER_PORT << '\n';

    std::cout
        << "[SERVER] Waiting for client connections...\n";

    // Main event loop.
    while (true)
    {
        const int eventCount = epoll_wait(
            epollFd,
            events.data(),
            static_cast<int>(events.size()),
            -1);

        if (eventCount < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "[ERROR] epoll_wait() failed: "
                << std::strerror(errno) << '\n';

            break;
        }

        for (int i = 0; i < eventCount; ++i)
        {
            const int readyFd = events[i].data.fd;

            if (readyFd == listenFd)
            {
                AcceptClients(
                    listenFd,
                    epollFd,
                    orderBook,
                    clients);

                continue;
            }

            ProcessClientEvents(
                epollFd,
                readyFd,
                events[i].events,
                clients);
        }
    }

    for (const auto& entry : clients)
    {
        close(entry.first);
    }

    close(epollFd);
    close(listenFd);

    return 0;
}