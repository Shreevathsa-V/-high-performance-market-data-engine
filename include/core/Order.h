#pragma once

#include <stdexcept>

#include "OrderType.h"
#include "Side.h"
#include "Usings.h"

class Order
{
public:
    Order(
        OrderType orderType,
        OrderId orderId,
        Side side,
        Price price,
        Quantity quantity)
        : orderType_{orderType}
        , orderId_{orderId}
        , side_{side}
        , price_{price}
        , initialQuantity_{quantity}
        , remainingQuantity_{quantity}
    {
        if (quantity == 0)
        {
            throw std::invalid_argument("Order quantity must be greater than zero.");
        }
    }

    OrderId GetOrderId() const noexcept
    {
        return orderId_;
    }

    OrderType GetOrderType() const noexcept
    {
        return orderType_;
    }

    Side GetSide() const noexcept
    {
        return side_;
    }

    Price GetPrice() const noexcept
    {
        return price_;
    }

    Quantity GetInitialQuantity() const noexcept
    {
        return initialQuantity_;
    }

    Quantity GetRemainingQuantity() const noexcept
    {
        return remainingQuantity_;
    }

    Quantity GetFilledQuantity() const noexcept
    {
        return initialQuantity_ - remainingQuantity_;
    }

    bool IsFilled() const noexcept
    {
        return remainingQuantity_ == 0;
    }

    void Fill(Quantity quantity)
    {
        if (quantity == 0)
        {
            throw std::invalid_argument("Fill quantity must be greater than zero.");
        }

        if (quantity > remainingQuantity_)
        {
            throw std::logic_error("Cannot fill more than the remaining quantity.");
        }

        remainingQuantity_ -= quantity;
    }

    void Modify(Price price, Quantity quantity)
    {
        if (quantity == 0)
            throw std::invalid_argument("Modified quantity must be greater than zero.");

        price_ = price;
        initialQuantity_ = quantity;
        remainingQuantity_ = quantity;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;

    Price price_;

    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};