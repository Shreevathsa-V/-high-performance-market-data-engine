#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "Order.h"
#include "OrderBook.h"
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

int main()
{
    constexpr std::size_t ORDER_COUNT = 100'000;
    constexpr std::size_t WARMUP_COUNT = 10'000;
    constexpr std::size_t REPETITIONS = 10;

    constexpr Price SELL_PRICE = 10'000;
    constexpr Price BUY_PRICE = 10'100;
    constexpr Quantity ORDER_QUANTITY = 100;

    // Warm up the matching path using a separate order book.
    {
        OrderBook warmupBook(false);

        for (std::size_t i = 0; i < WARMUP_COUNT; ++i)
        {
            warmupBook.AddOrder(std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(i + 1),
                Side::Sell,
                SELL_PRICE,
                ORDER_QUANTITY));
        }

        for (std::size_t i = 0; i < WARMUP_COUNT; ++i)
        {
            warmupBook.AddOrder(std::make_shared<Order>(
                OrderType::GoodTillCancel,
                static_cast<OrderId>(WARMUP_COUNT + i + 1),
                Side::Buy,
                BUY_PRICE,
                ORDER_QUANTITY));
        }

        if (!warmupBook.Empty() || !warmupBook.ValidateBook())
        {
            std::cerr << "Warm-up validation failed.\n";
            return EXIT_FAILURE;
        }
    }

    // Disable event logging to focus on matching and book operations.
    // Each repetition uses fresh orders with unique IDs.
    OrderBook book(false);

    std::vector<std::shared_ptr<Order>> restingOrders;
    std::vector<std::shared_ptr<Order>> incomingOrders;

    restingOrders.reserve(ORDER_COUNT);
    incomingOrders.reserve(ORDER_COUNT);

    std::uint64_t completedOrders = 0;

    for (std::size_t repetition = 0;
         repetition < REPETITIONS;
         ++repetition)
    {
        restingOrders.clear();
        incomingOrders.clear();

        const OrderId baseId =
            static_cast<OrderId>(
                WARMUP_COUNT + repetition * 2 * ORDER_COUNT + 1);

        // Prepare fresh resting sell orders.
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            restingOrders.push_back(std::make_shared<Order>(
                OrderType::GoodTillCancel,
                baseId + static_cast<OrderId>(i),
                Side::Sell,
                SELL_PRICE,
                ORDER_QUANTITY));
        }

        // Prepare fresh incoming buy orders before matching.
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            incomingOrders.push_back(std::make_shared<Order>(
                OrderType::GoodTillCancel,
                baseId + static_cast<OrderId>(ORDER_COUNT + i),
                Side::Buy,
                BUY_PRICE,
                ORDER_QUANTITY));
        }

        // Populate the book with resting sells.
        for (const auto& order : restingOrders)
        {
            book.AddOrder(order);
        }

        if (book.Size() != ORDER_COUNT || !book.ValidateBook())
        {
            std::cerr << "Initial book validation failed at repetition "
                      << repetition << ".\n";
            return EXIT_FAILURE;
        }

        // Match the pre-created incoming orders.
        for (std::size_t i = 0; i < ORDER_COUNT; ++i)
        {
            const auto& incoming = incomingOrders[i];
            const auto result = book.AddOrder(incoming);

            if (!incoming->IsFilled() || result.trades.empty())
            {
                std::cerr << "Unexpected matching result at repetition "
                          << repetition << ", order " << i << ".\n";
                return EXIT_FAILURE;
            }

            ++completedOrders;
        }

        if (!book.Empty() || !book.ValidateBook())
        {
            std::cerr << "Book validation failed after repetition "
                      << repetition << ".\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "Completed incoming orders: "
              << completedOrders << '\n'
              << "Final book size: " << book.Size() << '\n'
              << "Final book valid: " << std::boolalpha
              << book.ValidateBook() << '\n';

    return book.Empty() && book.ValidateBook()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
