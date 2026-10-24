#include <cassert>
#include <iostream>
#include <memory>

#include "OrderBook.h"

std::shared_ptr<Order> MakeFokTestOrder(
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
    // TEST 1: FOK must aggregate liquidity across levels.
    // =====================================================

    {
        OrderBook book(false);

        auto sell1 = MakeFokTestOrder(
            OrderType::GoodTillCancel, 101,
            Side::Sell, 10000, 40);

        auto sell2 = MakeFokTestOrder(
            OrderType::GoodTillCancel, 102,
            Side::Sell, 10100, 30);

        auto sell3 = MakeFokTestOrder(
            OrderType::GoodTillCancel, 103,
            Side::Sell, 10200, 50);

        auto fok = MakeFokTestOrder(
            OrderType::FillOrKill, 104,
            Side::Buy, 10200, 100);

        book.AddOrder(sell1);
        book.AddOrder(sell2);
        book.AddOrder(sell3);

        auto result = book.AddOrder(fok);

        assert(result.trades.size() == 3);

        assert(sell1->GetRemainingQuantity() == 0);
        assert(sell2->GetRemainingQuantity() == 0);
        assert(sell3->GetRemainingQuantity() == 20);
        assert(fok->GetRemainingQuantity() == 0);

        assert(result.trades[0].ask.orderId == 101);
        assert(result.trades[0].ask.price == 10000);
        assert(result.trades[0].ask.quantity == 40);

        assert(result.trades[1].ask.orderId == 102);
        assert(result.trades[1].ask.price == 10100);
        assert(result.trades[1].ask.quantity == 30);

        assert(result.trades[2].ask.orderId == 103);
        assert(result.trades[2].ask.price == 10200);
        assert(result.trades[2].ask.quantity == 30);

        assert(book.Size() == 1);
        assert(book.GetBestAsk() == 10200);
        assert(book.GetBestAskQuantity() == 20);
        assert(book.ValidateBook());

        std::cout
            << "TEST 1 PASSED: FOK across multiple price levels\n";
    }

    // =====================================================
    // TEST 2: FOK must not execute if total liquidity is
    // insufficient, even when multiple levels are available.
    // =====================================================

    {
        OrderBook book(false);

        auto sell1 = MakeFokTestOrder(
            OrderType::GoodTillCancel, 201,
            Side::Sell, 10000, 40);

        auto sell2 = MakeFokTestOrder(
            OrderType::GoodTillCancel, 202,
            Side::Sell, 10100, 30);

        auto sell3 = MakeFokTestOrder(
            OrderType::GoodTillCancel, 203,
            Side::Sell, 10200, 50);

        auto fok = MakeFokTestOrder(
            OrderType::FillOrKill, 204,
            Side::Buy, 10200, 130);

        book.AddOrder(sell1);
        book.AddOrder(sell2);
        book.AddOrder(sell3);

        auto result = book.AddOrder(fok);

        assert(result.trades.empty());

        assert(sell1->GetRemainingQuantity() == 40);
        assert(sell2->GetRemainingQuantity() == 30);
        assert(sell3->GetRemainingQuantity() == 50);
        assert(fok->GetRemainingQuantity() == 130);

        assert(book.Size() == 3);
        assert(book.GetBestAsk() == 10000);
        assert(book.ValidateBook());

        std::cout
            << "TEST 2 PASSED: Insufficient multi-level FOK liquidity\n";
    }

    std::cout << "\nAll multi-level FOK tests passed!\n";

    return 0;
}