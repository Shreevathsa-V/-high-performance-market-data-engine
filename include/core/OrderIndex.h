#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "Order.h"
#include "OrderNodePool.h"
#include "Usings.h"

class OrderIndex
{
public:
    using OrderPointer = std::shared_ptr<Order>;
    using Node = OrderNodePool::Node;

    struct Entry
    {
        OrderId orderId{0};
        OrderPointer order;
        Node* location{nullptr};

        Entry* next{nullptr};
    };

    explicit OrderIndex(
        std::size_t capacity,
        std::size_t bucketCount)
        : entries_(capacity)
        , buckets_(bucketCount, nullptr)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument(
                "OrderIndex capacity must be greater than zero.");
        }

        if (bucketCount == 0)
        {
            throw std::invalid_argument(
                "OrderIndex bucket count must be greater than zero.");
        }

        // Build the free-entry linked list.
        for (std::size_t i = 0; i + 1 < capacity; ++i)
        {
            entries_[i].next = &entries_[i + 1];
        }

        entries_[capacity - 1].next = nullptr;

        freeListHead_ = &entries_[0];
    }

    OrderIndex(const OrderIndex&) = delete;
    OrderIndex& operator=(const OrderIndex&) = delete;

    Entry* Find(OrderId orderId) noexcept
    {
        Entry* current =
            buckets_[GetBucketIndex(orderId)];

        while (current)
        {
            if (current->orderId == orderId)
            {
                return current;
            }

            current = current->next;
        }

        return nullptr;
    }

    const Entry* Find(OrderId orderId) const noexcept
    {
        const Entry* current =
            buckets_[GetBucketIndex(orderId)];

        while (current)
        {
            if (current->orderId == orderId)
            {
                return current;
            }

            current = current->next;
        }

        return nullptr;
    }

    bool Contains(OrderId orderId) const noexcept
    {
        return Find(orderId) != nullptr;
    }

    Entry* Insert(
        OrderId orderId,
        const OrderPointer& order,
        Node* location)
    {
        if (!order)
        {
            throw std::invalid_argument(
                "Cannot insert a null order.");
        }

        if (Find(orderId))
        {
            throw std::logic_error(
                "Order ID already exists.");
        }

        if (!freeListHead_)
        {
            throw std::runtime_error(
                "OrderIndex exhausted.");
        }

        Entry* entry = freeListHead_;

        freeListHead_ = entry->next;

        entry->orderId = orderId;
        entry->order = order;
        entry->location = location;

        const std::size_t bucket =
            GetBucketIndex(orderId);

        entry->next = buckets_[bucket];
        buckets_[bucket] = entry;

        ++size_;

        return entry;
    }

    void Erase(OrderId orderId)
    {
        const std::size_t bucket =
            GetBucketIndex(orderId);

        Entry* current = buckets_[bucket];
        Entry* previous = nullptr;

        while (current)
        {
            if (current->orderId == orderId)
            {
                if (previous)
                {
                    previous->next = current->next;
                }
                else
                {
                    buckets_[bucket] = current->next;
                }

                current->orderId = 0;
                current->order.reset();
                current->location = nullptr;

                current->next = freeListHead_;
                freeListHead_ = current;

                --size_;

                return;
            }

            previous = current;
            current = current->next;
        }

        throw std::logic_error(
            "Cannot erase order that does not exist.");
    }

    template <typename Function>
    void ForEach(Function&& function) const
    {
        for (const Entry& entry : entries_)
        {
            if (entry.order)
            {
                function(
                    entry.orderId,
                    entry.order,
                    entry.location);
            }
        }
    }

    std::size_t Size() const noexcept
    {
        return size_;
    }

private:
    std::size_t GetBucketIndex(OrderId orderId) const noexcept
    {
        // Mix the order ID bits before reducing to a bucket index.
        std::uint64_t hash = static_cast<std::uint64_t>(orderId);

        hash ^= hash >> 30;
        hash *= 0xbf58476d1ce4e5b9ULL;
        hash ^= hash >> 27;
        hash *= 0x94d049bb133111ebULL;
        hash ^= hash >> 31;

        return static_cast<std::size_t>(
            hash % buckets_.size());
    }

    std::vector<Entry> entries_;
    std::vector<Entry*> buckets_;

    Entry* freeListHead_{nullptr};

    std::size_t size_{0};
};