#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "OrderBook.h"

std::shared_ptr<Order> MakeRandomOrder(
    OrderId id,
    Side side,
    Price price,
    Quantity quantity)
{
    return std::make_shared<Order>(
        OrderType::GoodTillCancel,
        id,
        side,
        price,
        quantity);
}

int main()
{
    OrderBook book(false);

    // Fixed seed makes the test reproducible.
    std::mt19937_64 rng(123456789);

    std::uniform_int_distribution<int> operationDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> priceDist(9900, 10100);
    std::uniform_int_distribution<int> quantityDist(1, 100);

    std::vector<OrderId> generatedOrderIds;

    constexpr int TOTAL_OPERATIONS = 20000;

    OrderId nextOrderId = 1;

    int addAttempts = 0;
    int cancelAttempts = 0;
    int successfulCancellations = 0;

    for (int i = 0; i < TOTAL_OPERATIONS; ++i)
    {
        const int operation = operationDist(rng);

        if (operation <= 65 || generatedOrderIds.empty())
        {
            // Approximately 65% of operations add an order.

            const Side side =
                sideDist(rng) == 0 ? Side::Buy : Side::Sell;

            const Price price = priceDist(rng);

            const Quantity quantity =
                static_cast<Quantity>(quantityDist(rng));

            auto order = MakeRandomOrder(
                nextOrderId,
                side,
                price,
                quantity);

            generatedOrderIds.push_back(nextOrderId);

            ++nextOrderId;
            ++addAttempts;

            book.AddOrder(order);
        }
        else
        {
            // Approximately 35% of operations attempt cancellation.
            // The selected ID may already have been filled or cancelled.

            std::uniform_int_distribution<std::size_t> idDist(
                0,
                generatedOrderIds.size() - 1);

            const OrderId orderId =
                generatedOrderIds[idDist(rng)];

            ++cancelAttempts;

            if (book.CancelOrder(orderId))
            {
                ++successfulCancellations;
            }
        }

        // Verify the book after every operation.
        if (!book.ValidateBook())
        {
            std::cerr
                << "INVARIANT FAILURE at operation "
                << (i + 1)
                << '\n';

            std::cerr
                << "Book size: "
                << book.Size()
                << '\n';

            return 1;
        }
    }

    std::cout << "Randomized stress test completed.\n";

    std::cout
        << "Total operations: "
        << TOTAL_OPERATIONS
        << '\n';

    std::cout
        << "Order additions: "
        << addAttempts
        << '\n';

    std::cout
        << "Cancellation attempts: "
        << cancelAttempts
        << '\n';

    std::cout
        << "Successful cancellations: "
        << successfulCancellations
        << '\n';

    std::cout
        << "Final active orders: "
        << book.Size()
        << '\n';

    std::cout
        << "Final invariant check: "
        << (book.ValidateBook() ? "PASSED" : "FAILED")
        << '\n';

    std::cout
        << "\nAll randomized stress-test operations completed successfully!\n";

    return 0;
}