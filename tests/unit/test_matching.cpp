#include <cassert>
#include <iostream>
#include <memory>

#include "OrderBook.h"

std::shared_ptr<Order> MakeOrder(
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
    // =====================================================
    // TEST 1: Partial fill
    // Sell 50, then buy 80 at the same price.
    // Expected: sell fills completely; buy has 30 remaining.
    // =====================================================

    OrderBook book(false);

    auto sell1 = MakeOrder(1, Side::Sell, 10000, 50);
    auto buy1 = MakeOrder(2, Side::Buy, 10000, 80);

    book.AddOrder(sell1);

    auto result = book.AddOrder(buy1);

    assert(sell1->GetRemainingQuantity() == 0);
    assert(buy1->GetRemainingQuantity() == 30);

    assert(result.trades.size() == 1);

    assert(result.trades[0].bid.orderId == 2);
    assert(result.trades[0].ask.orderId == 1);

    assert(result.trades[0].bid.price == 10000);
    assert(result.trades[0].ask.price == 10000);

    assert(result.trades[0].bid.quantity == 50);
    assert(result.trades[0].ask.quantity == 50);

    assert(book.Size() == 1);
    assert(book.GetBestBid() == 10000);
    assert(book.GetBestBidQuantity() == 30);
    assert(book.ValidateBook());

    std::cout << "TEST 1 PASSED: Partial fill\n";

    // =====================================================
    // TEST 2: FIFO priority at the same price.
    //
    // Sell order 10: 40 units at 10100.
    // Sell order 11: 60 units at 10100.
    // Buy order 12:  70 units at 10100.
    //
    // Expected:
    // Order 10 fills 40 first.
    // Order 11 then fills 30 and has 30 remaining.
    // =====================================================

    OrderBook fifoBook(false);

    auto sell2 = MakeOrder(10, Side::Sell, 10100, 40);
    auto sell3 = MakeOrder(11, Side::Sell, 10100, 60);
    auto buy2 = MakeOrder(12, Side::Buy, 10100, 70);

    fifoBook.AddOrder(sell2);
    fifoBook.AddOrder(sell3);

    auto fifoResult = fifoBook.AddOrder(buy2);

    assert(fifoResult.trades.size() == 2);

    // First trade must execute against order 10.
    assert(fifoResult.trades[0].ask.orderId == 10);
    assert(fifoResult.trades[0].bid.orderId == 12);
    assert(fifoResult.trades[0].ask.quantity == 40);
    assert(fifoResult.trades[0].bid.quantity == 40);

    // Second trade must execute against order 11.
    assert(fifoResult.trades[1].ask.orderId == 11);
    assert(fifoResult.trades[1].bid.orderId == 12);
    assert(fifoResult.trades[1].ask.quantity == 30);
    assert(fifoResult.trades[1].bid.quantity == 30);

    assert(sell2->GetRemainingQuantity() == 0);
    assert(sell3->GetRemainingQuantity() == 30);
    assert(buy2->GetRemainingQuantity() == 0);

    assert(fifoBook.Size() == 1);
    assert(fifoBook.GetBestAsk() == 10100);
    assert(fifoBook.GetBestAskQuantity() == 30);
    assert(fifoBook.ValidateBook());

    std::cout << "TEST 2 PASSED: FIFO price-time priority\n";

    // =====================================================
    // TEST 3: Cancel the partially filled resting order.
    // =====================================================

    assert(fifoBook.CancelOrder(11));

    assert(fifoBook.Empty());
    assert(fifoBook.ValidateBook());

    std::cout << "TEST 3 PASSED: Cancellation after partial fill\n";

    std::cout << "\nAll matching tests passed!\n";

    return 0;
}