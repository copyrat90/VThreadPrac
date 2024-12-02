#pragma once

#include "MemoryLogger.hpp"

#include <atomic>
#include <cstdint>
#include <optional>

namespace vtp
{

template <typename T>
class Stack
{
private:
    struct Node
    {
        T data;
        Node* next;
    };

public:
    void push(const T& data)
    {
        Node* old_top = _top.load();
        Node* new_node = new Node{data, old_top};
        while (!_top.compare_exchange_weak(new_node->next, new_node))
            ;
        _logger.log("insert new_node: ",  reinterpret_cast<std::uintptr_t>(new_node));
    }

    auto pop() -> std::optional<T>
    {
        std::optional<T> result;

        Node* old_top = _top.load();
        _logger.log("getting old_top: ", reinterpret_cast<std::uintptr_t>(old_top));

        while (old_top && !_top.compare_exchange_weak(old_top, old_top->next))
            _logger.log("re-getting old_top: ", reinterpret_cast<std::uintptr_t>(old_top));

        _logger.log("got old_top: ", reinterpret_cast<std::uintptr_t>( old_top));

        if (old_top)
        {
            const auto old_top_addr = reinterpret_cast<std::uintptr_t>(old_top);
            result = old_top->data;
            delete old_top;
            _logger.log("delete old_top: ", old_top_addr);
        }

        return result;
    }

private:
    std::atomic<Node*> _top;

    static_assert(decltype(_top)::is_always_lock_free);

    MemoryLogger<std::uintptr_t> _logger;
};

} // namespace vtp
