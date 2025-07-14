#pragma once

#include <deque>
#include <algorithm>
#include <iostream>

// Custom queue class that allows removal of specific items
template<typename T>
class RemovableQueue {
private:
    std::deque<T> container;  // deque allows both front/back and middle access

public:
    // Queue-like operations
    void push(const T& item)
    {
        container.push_back(item);
    }

    void push(T&& item)
    {
        container.push_back(std::move(item));
    }

    T front() const
    {
        return container.front();
    }

    void pop()
    {
        container.pop_front();
    }

    bool empty() const
    {
        return container.empty();
    }

    size_t size() const
    {
        return container.size();
    }

    // Remove specific item by value
    bool remove(const T& item)
    {
        auto it = std::find(container.begin(), container.end(), item);
        if (it != container.end())
        {
            container.erase(it);
            return true;  // Item found and removed
        }
        return false;  // Item not found
    }

    // Remove all matching items
    size_t removeAll(const T& item)
    {
        size_t count = 0;
        auto it = container.begin();
        while (it != container.end())
        {
            if (*it == item)
            {
                it = container.erase(it);
                count++;
            }
            else
            {
                ++it;
            }
        }
        return count;
    }

    // Remove item by condition (like LINQ Where in C#)
    template<typename Predicate>
    size_t removeIf(Predicate pred)
    {
        size_t originalSize = container.size();
        container.erase(
            std::remove_if(container.begin(), container.end(), pred),
            container.end()
        );
        return originalSize - container.size();
    }

    // Check if item exists
    bool contains(const T& item) const
    {
        return std::find(container.begin(), container.end(), item) != container.end();
    }

    // Access by index (for debugging)
    const T& operator[](size_t index) const
    {
        return container[index];
    }
};