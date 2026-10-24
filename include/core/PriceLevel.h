#pragma once

#include <cstddef>
#include <stdexcept>

#include "OrderNodePool.h"

class PriceLevel
{
public:
    using OrderPointer = OrderNodePool::OrderPointer;
    using Node = OrderNodePool::Node;

    explicit PriceLevel(Price price)
        : price_{price}
    {
    }

    Price GetPrice() const noexcept
    {
        return price_;
    }

    Quantity GetTotalQuantity() const noexcept
    {
        return totalQuantity_;
    }

    bool Empty() const noexcept
    {
        return head_ == nullptr;
    }

    std::size_t Size() const noexcept
    {
        return size_;
    }

    Node* GetHead() noexcept
    {
        return head_;
    }

    const Node* GetHead() const noexcept
    {
        return head_;
    }

    Node* GetTail() noexcept
    {
        return tail_;
    }

    const Node* GetTail() const noexcept
    {
        return tail_;
    }

    void AddNode(Node* node)
    {
        if (!node)
        {
            throw std::invalid_argument(
                "Cannot add a null node.");
        }

        node->next = nullptr;
        node->previous = tail_;

        if (tail_)
        {
            tail_->next = node;
        }
        else
        {
            head_ = node;
        }

        tail_ = node;

        totalQuantity_ +=
            node->order->GetRemainingQuantity();

        ++size_;
    }

    void RemoveNode(Node* node)
    {
        if (!node)
        {
            throw std::invalid_argument(
                "Cannot remove a null node.");
        }

        if (node->previous)
        {
            node->previous->next = node->next;
        }
        else
        {
            head_ = node->next;
        }

        if (node->next)
        {
            node->next->previous = node->previous;
        }
        else
        {
            tail_ = node->previous;
        }

        totalQuantity_ -=
            node->order->GetRemainingQuantity();

        node->next = nullptr;
        node->previous = nullptr;

        --size_;
    }

    void ReduceQuantity(Quantity quantity)
    {
        if (quantity > totalQuantity_)
        {
            throw std::logic_error(
                "Cannot reduce price level by more than its quantity.");
        }

        totalQuantity_ -= quantity;
    }

private:
    Price price_;

    Quantity totalQuantity_{0};

    Node* head_{nullptr};
    Node* tail_{nullptr};

    std::size_t size_{0};
};