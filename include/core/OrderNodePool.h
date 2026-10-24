#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Order.h"

class OrderNodePool
{
public:
    using OrderPointer = std::shared_ptr<Order>;

    struct Node
    {
        OrderPointer order;
        Node* next{nullptr};
        Node* previous{nullptr};
    };

    explicit OrderNodePool(std::size_t capacity)
        : nodes_(capacity)
    {
        for (std::size_t i = 0; i + 1 < capacity; ++i)
        {
            nodes_[i].next = &nodes_[i + 1];
        }

        if (capacity > 0)
        {
            nodes_[capacity - 1].next = nullptr;
            freeListHead_ = &nodes_[0];
        }
    }

    Node* Acquire(const OrderPointer& order)
    {
        if (!freeListHead_)
        {
            throw std::runtime_error(
                "OrderNodePool exhausted.");
        }

        Node* node = freeListHead_;
        freeListHead_ = node->next;

        node->order = order;
        node->next = nullptr;
        node->previous = nullptr;

        return node;
    }

    void Release(Node* node)
    {
        if (!node)
        {
            return;
        }

        node->order.reset();
        node->previous = nullptr;

        node->next = freeListHead_;
        freeListHead_ = node;
    }

private:
    std::vector<Node> nodes_;
    Node* freeListHead_{nullptr};
};