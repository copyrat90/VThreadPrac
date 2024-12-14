#pragma once

#include "Queue_fwd.hpp"

#include "MemoryLogger.hpp" // test

#include <NetBuff/LockfreeObjectPool.hpp>
#include <NetBuff/TaggedPtr.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>

// test
#define VTP_Q_STOP_ON_FAIL \
    do \
    { \
        if (_failed.load()) \
            throw std::logic_error("stopped"); \
    } while (false);

namespace vtp
{

template <typename T, typename Allocator>
class Queue
{
private:
    struct Node
    {
    private:
        // workaround `alignof(Node)` not usable in `Node`
        static constexpr auto ALIGNMENT = std::max(alignof(T), alignof(std::atomic<std::uintptr_t>));

    public:
        std::atomic<nb::TaggedPtrAligned<Node, ALIGNMENT>> next;

        alignas(T) std::byte data[sizeof(T)];

    public:
        auto obj() -> T&
        {
            return reinterpret_cast<T&>(data);
        }

        auto obj() const -> const T&
        {
            return reinterpret_cast<const T&>(data);
        }
    };

    static_assert(std::atomic<nb::TaggedPtr<Node>>::is_always_lock_free);

public:
    /// @param capacity reserved capacity for internal object pool
    explicit Queue(std::size_t capacity = 0) : _node_pool(capacity)
    {
        Node& dummy = _node_pool.construct();
        nb::TaggedPtr<Node> tagged_dummy(&dummy);

        _head.store(tagged_dummy);
        _tail.store(tagged_dummy);
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

public:
    ~Queue()
    {
        while (pop())
            ;
    }

public: // Capacity
    auto size() const noexcept -> std::size_t
    {
        return _size;
    }

public: // Modifiers
    long push(const T& value)
    {
        return emplace(value);
    }

    long push(T&& value)
    {
        return emplace(std::move(value));
    }

    template <typename... Args>
    long emplace(Args&&... args)
    {
        Node& new_node = _node_pool.construct();
        ::new (static_cast<void*>(new_node.data)) T(std::forward<Args>(args)...);

        for (std::size_t i = 0;; ++i)
        {
            if (i > 0xFFFFFFFF) // infinite loop test
            {
                _failed.store(true);
                throw std::logic_error("infinite loop");
            }

            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_tail = _tail.load();
            _logger.log(i == 0 ? "getting old_tail:" : "re-getting old_tail:", old_tail);
            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_tail_next = old_tail->next.load();
            _logger.log(i == 0 ? "getting old_tail_next:" : "re-getting old_tail_next:", old_tail_next);

            // check if `old_tail_next` got from `old_tail` is valid
            // via checking `old_tail` was current `_tail`
            if (_tail.load() != old_tail)
                continue;

            if (!old_tail_next)
            {
                nb::TaggedPtr<Node> new_tail_next(&new_node, old_tail_next.get_tag() + 1);
                VTP_Q_STOP_ON_FAIL;
                if (old_tail->next.compare_exchange_weak(old_tail_next, new_tail_next))
                {
                    _logger.log("push new_tail_next:", new_tail_next);
                    nb::TaggedPtr<Node> new_tail(&new_node, old_tail.get_tag() + 1);
                    VTP_Q_STOP_ON_FAIL;
                    const bool success = _tail.compare_exchange_strong(old_tail, new_tail);
                    if (success) // test
                    {
                        _logger.log("push new_tail:", new_tail);
                    }
                    else
                    {
                        _failed.store(true);
                        _logger.log("cas2 failed!", old_tail);
                        throw std::logic_error("push_cas_2_failed");
                    }
                    break;
                }
            }
        }

        return ++_size;
    }

    auto pop() -> std::optional<T>
    {
        std::optional<T> result;

        for (std::size_t i = 0;; ++i)
        {
            if (i > 0xFFFFFFFF) // infinite loop test
            {
                _failed.store(true);
                throw std::logic_error("infinite loop");
            }

            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_head = _head.load();
            _logger.log(i == 0 ? "getting old_head:" : "re-getting old_head:", old_head);
            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_head_next = old_head->next.load();
            _logger.log(i == 0 ? "getting old_head_next:" : "re-getting old_head_next:", old_head_next);

            // check if `old_head_next` got from `old_head` is valid
            // via checking `old_head` was current `_head`
            if (_head.load() != old_head)
                continue;

            if (!old_head_next)
            {
                _logger.log("q was empty!", nb::TaggedPtr<Node>());
                break;
            }

            nb::TaggedPtr<Node> new_head(old_head_next.get_ptr(), old_head.get_tag() + 1);

            VTP_Q_STOP_ON_FAIL;
            if (_head.compare_exchange_weak(old_head, new_head))
            {
                const auto size = --_size;
                if (size < 0)
                {
                    _failed.store(true);
                    _logger.log("size is negative", nb::TaggedPtr<Node>(nullptr, static_cast<std::uintptr_t>(size)));
                    throw std::logic_error("old_head_next reset failed");
                }
                _logger.log("popped:", old_head);

                // data is in `old_head_next`
                _logger.log("extracting data from old_head_next:", old_head_next);
                result.emplace(std::move(old_head_next->obj()));
                old_head_next->obj().~T();
                _logger.log("extracted data from old_head_next:", old_head_next);

                // dealloc `old_head`
                _logger.log("deallocating old_head:", old_head);
                nb::TaggedPtr<Node> new_old_head_next(nullptr, old_head_next.get_tag());
                if (!old_head->next.compare_exchange_strong(old_head_next, new_old_head_next))
                {
                    _failed.store(true);
                    _logger.log("old_head_next reset failed!", old_head_next);
                    throw std::logic_error("old_head_next reset failed");
                }
                _node_pool.destroy(*old_head);
                _logger.log("deallocated old_head:", old_head);
                break;
            }
        }

        return result;
    }

private:
    nb::LockfreeObjectPool<Node, false, Allocator> _node_pool;

    std::atomic<nb::TaggedPtr<Node>> _head;
    std::atomic<nb::TaggedPtr<Node>> _tail;

    std::atomic<long> _size = 0;

    static_assert(decltype(_size)::is_always_lock_free);

private: // test
    MemoryLogger<nb::TaggedPtr<Node>, 1 << 16> _logger;

public: // test
    std::atomic<bool> _failed;

    static_assert(std::atomic<bool>::is_always_lock_free);
};

} // namespace vtp
