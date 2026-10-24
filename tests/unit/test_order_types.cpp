#include <cassert>
#include <iostream>
#include <memory>

#include "OrderBook.h"

std::shared_ptr<Order> MakeTestOrder(
    OrderType type,
    OrderId id,
    Side side,
    Price price,
    Quantity quantity)
{
    return std::make_shared<Order>(
        type,
        id,
        side,
        price,
        quantity);
}

int main()
{
    // =====================================================
    // TEST 1: Fill-or-Kill (FOK)
    //
    // An FOK order must fill completely or not execute.
    // =====================================================

    {
        OrderBook book(false);

        auto sell = MakeTestOrder(
            OrderType::GoodTillCancel,
            1,
            Side::Sell,
            10000,
            50);

        auto fok = MakeTestOrder(
            OrderType::FillOrKill,
            2,
            Side::Buy,
            10000,
            100);

        book.AddOrder(sell);

        auto result = book.AddOrder(fok);

        assert(result.trades.empty());
        assert(sell->GetRemainingQuantity() == 50);
        assert(fok->GetRemainingQuantity() == 100);
        assert(book.Size() == 1);
        assert(book.ValidateBook());

        std::cout << "TEST 1 PASSED: FOK cannot fill completely\n";
    }

    // =====================================================
    // TEST 2: Successful Fill-or-Kill
    // =====================================================

    {
        OrderBook book(false);

        auto sell1 = MakeTestOrder(
            OrderType::GoodTillCancel,
            10,
            Side::Sell,
            10000,
            40);

        auto sell2 = MakeTestOrder(
            OrderType::GoodTillCancel,
            11,
            Side::Sell,
            10000,
            60);

        auto fok = MakeTestOrder(
            OrderType::FillOrKill,
            12,
            Side::Buy,
            10000,
            100);

        book.AddOrder(sell1);
        book.AddOrder(sell2);

        auto result = book.AddOrder(fok);

        assert(result.trades.size() == 2);
        assert(sell1->GetRemainingQuantity() == 0);
        assert(sell2->GetRemainingQuantity() == 0);
        assert(fok->GetRemainingQuantity() == 0);
        assert(book.Empty());
        assert(book.ValidateBook());

        std::cout << "TEST 2 PASSED: Successful FOK execution\n";
    }

    // =====================================================
    // TEST 3: Fill-and-Kill (FAK)
    //
    // Execute available quantity immediately.
    // Cancel the unfilled remainder.
    // =====================================================

    {
        OrderBook book(false);

        auto sell = MakeTestOrder(
            OrderType::GoodTillCancel,
            20,
            Side::Sell,
            10000,
            40);

        auto fak = MakeTestOrder(
            OrderType::FillAndKill,
            21,
            Side::Buy,
            10000,
            100);

        book.AddOrder(sell);

        auto result = book.AddOrder(fak);

        assert(result.trades.size() == 1);
        assert(sell->GetRemainingQuantity() == 0);
        assert(fak->GetRemainingQuantity() == 60);

        // The unfilled FAK remainder must not rest in the book.
        assert(book.Empty());
        assert(book.ValidateBook());

        std::cout << "TEST 3 PASSED: FAK cancels unfilled remainder\n";
    }

    // =====================================================
    // TEST 4: Reduce quantity without changing price
    //
    // The order should retain its FIFO position.
    // =====================================================

    {
        OrderBook book(false);

        auto sell = MakeTestOrder(
            OrderType::GoodTillCancel,
            30,
            Side::Sell,
            10000,
            100);

        book.AddOrder(sell);

        OrderModify modify{
            30,
            10000,
            60
        };

        auto result = book.ModifyOrder(modify);

        assert(result.trades.empty());
        assert(sell->GetRemainingQuantity() == 60);
        assert(book.Size() == 1);
        assert(book.GetBestAskQuantity() == 60);
        assert(book.ValidateBook());

        std::cout << "TEST 4 PASSED: Quantity reduction\n";
    }

    // =====================================================
    // TEST 5: Increase quantity
    //
    // The order should be removed and reinserted.
    // =====================================================

    {
        OrderBook book(false);

        auto sell = MakeTestOrder(
            OrderType::GoodTillCancel,
            40,
            Side::Sell,
            10000,
            50);

        book.AddOrder(sell);

        OrderModify modify{
            40,
            10000,
            80
        };

        book.ModifyOrder(modify);

        assert(sell->GetRemainingQuantity() == 80);
        assert(book.Size() == 1);
        assert(book.GetBestAskQuantity() == 80);
        assert(book.ValidateBook());

        std::cout << "TEST 5 PASSED: Quantity increase\n";
    }

    std::cout << "\nAll order-type and modification tests passed!\n";

    return 0;
}