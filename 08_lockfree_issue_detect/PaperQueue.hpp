#pragma once

#include "PaperQueue_fwd.hpp"

#include "MemoryLogger.hpp" // test

#include <NetBuff/LockfreeObjectPool.hpp>
#include <NetBuff/TaggedPtr.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

// test
#define VTP_Q_STOP_ON_FAIL \
    do \
    { \
        if (_failed.load()) \
            throw std::logic_error("stopped"); \
    } while (false);

namespace vtp
{

// test
static constexpr std::size_t Q_INFINITE_LOOP_COUNT = 0x4'0000'0000;

template <typename T, typename Allocator>
    requires std::is_trivially_destructible_v<T> && std::is_trivially_copy_constructible_v<T>
class PaperQueue
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
    explicit PaperQueue(std::size_t capacity = 0) : _node_pool(capacity)
    {
        Node& dummy = _node_pool.construct();
        nb::TaggedPtr<Node> tagged_dummy(&dummy);

        _head.store(tagged_dummy);
        _tail.store(tagged_dummy);
    }

    PaperQueue(const PaperQueue&) = delete;
    PaperQueue& operator=(const PaperQueue&) = delete;

public:
    ~PaperQueue()
    {
        while (pop())
            ;

        assert(_head.load() == _tail.load());
        // dealloc final dummy node
        _node_pool.destroy(*_head.load());
    }

public: // Capacity
    auto size() const noexcept -> std::size_t
    {
        return _size;
    }

public: // Modifiers
    auto push(const T& value)-> std::size_t
    {
        return emplace(value);
    }

    auto push(T&& value)-> std::size_t
    {
        return emplace(std::move(value));
    }

    template <typename... Args>
    auto emplace(Args&&... args) -> std::size_t
    {
        const auto size = ++_size;

        // alloc `adding_node` from pool
        Node& adding_node = _node_pool.construct();
        _logger.log("allocated adding_node:", nb::TaggedPtr<Node>(&adding_node));

        // destroy `adding_node` on fail
        struct AddingNodeDestroyerOnFail
        {
            decltype((_node_pool)) pool;
            Node* node;

            AddingNodeDestroyerOnFail(Node* node_, decltype((_node_pool)) pool_) : pool(pool_), node(node_) {};

            ~AddingNodeDestroyerOnFail()
            {
                if (node)
                    pool.destroy(*node);
            }
        } adding_node_destroyer(&adding_node, _node_pool);

        // construct `T` in `adding_node`
        ::new (static_cast<void*>(adding_node.data)) T(std::forward<Args>(args)...);

        // clear the `adding_node.next` to `nullptr`
        nb::TaggedPtr<Node> old_node_next = adding_node.next.load(std::memory_order_acquire);
        nb::TaggedPtr<Node> new_node_next(nullptr, old_node_next.get_tag());
        _logger.log("clearing adding_node.next:", old_node_next);
        VTP_Q_STOP_ON_FAIL;
        adding_node.next.store(new_node_next, std::memory_order_relaxed);
        _logger.log("cleared adding_node.next to:", new_node_next);

        // try push loop
        for (std::size_t i = 0;; ++i)
        {
            if (i > Q_INFINITE_LOOP_COUNT) // infinite loop test
            {
                _failed.store(true);
                throw std::logic_error("possible INFINITE LOOP");
            }

            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_tail = _tail.load(std::memory_order_acquire);
            _logger.log(i == 0 ? "got old_tail:" : "re-got old_tail:", old_tail);
            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_tail_next = old_tail->next.load(std::memory_order_acquire);
            _logger.log(i == 0 ? "got old_tail_next:" : "re-got old_tail_next:", old_tail_next);

            // check if `old_tail_next` got from `old_tail` is valid
            // via checking `old_tail` was current `_tail`
            const nb::TaggedPtr<Node> old_tail_validate = _tail.load(std::memory_order_acquire);
            if (old_tail_validate != old_tail)
            {
                _logger.log("retry push, tail changed to:", old_tail_validate);
                continue;
            }

            if (!old_tail_next)
            {
                nb::TaggedPtr<Node> new_tail_next(&adding_node, old_tail_next.get_tag() + 1);
                _logger.log("try push new_tail_next:", new_tail_next);
                VTP_Q_STOP_ON_FAIL;
                if (old_tail->next.compare_exchange_weak(old_tail_next, new_tail_next, std::memory_order_release,
                                                         std::memory_order_relaxed))
                {
                    _logger.log("pushed new_tail_next:", new_tail_next);

                    // move tail
                    nb::TaggedPtr<Node> new_tail(&adding_node, old_tail.get_tag() + 1);
                    _logger.log("try move to new_tail:", new_tail);
                    VTP_Q_STOP_ON_FAIL;
                    const bool success = _tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release,
                                                                       std::memory_order_relaxed);

                    if (success) // test
                        _logger.log("moved to new_tail:", new_tail);
                    else
                        _logger.log("tail already moved by other thread, old_tail was:", old_tail);
                    break;
                }
            }
            else
            {
                _logger.log("retry push, old_tail_next exists:", old_tail_next);
                // failed, but update tail to recent observed one
                nb::TaggedPtr<Node> new_tail(old_tail_next.get_ptr(), old_tail.get_tag() + 1);
                VTP_Q_STOP_ON_FAIL;
                const bool success = _tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release,
                                                                   std::memory_order_relaxed);

                if (success) // test
                    _logger.log("btw, tail updated to:", new_tail);
                else
                    _logger.log("btw, tail was:", old_tail);

                continue;
            }
        }

        adding_node_destroyer.node = nullptr;
        return size;
    }

    auto pop() -> std::optional<T>
    {
        std::optional<T> result;

        for (std::size_t i = 0;; ++i)
        {
            if (i > Q_INFINITE_LOOP_COUNT) // infinite loop test
            {
                _failed.store(true);
                throw std::logic_error("possible INFINITE LOOP");
            }

            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_head = _head.load(std::memory_order_acquire);
            _logger.log(i == 0 ? "got old_head:" : "re-got old_head:", old_head);
            VTP_Q_STOP_ON_FAIL;
            nb::TaggedPtr<Node> old_tail = _tail.load(std::memory_order_acquire);
            nb::TaggedPtr<Node> old_head_next = old_head->next.load(std::memory_order_acquire);
            _logger.log(i == 0 ? "got old_head_next:" : "re-got old_head_next:", old_head_next);

            // check if `old_head_next` got from `old_head` is valid
            // via checking `old_head` was still current `_head`
            const nb::TaggedPtr<Node> old_head_validate = _head.load();
            if (old_head_validate != old_head)
            {
                _logger.log("retry pop, head changed to:", old_head_validate);
                continue;
            }

            // queue was empty / tail was not updated yet on `push()`
            if (old_head == old_tail)
            {
                // queue was empty
                if (!old_head_next)
                {
                    _logger.log("q was empty! old_head:", old_head);
                    return std::nullopt;
                }
                // tail was not updated yet on `push()`
                else
                {
                    // update to recent observed one
                    nb::TaggedPtr<Node> new_tail(old_head_next.get_ptr(), old_tail.get_tag() + 1);
                    const bool success = _tail.compare_exchange_strong(old_tail, new_tail, std::memory_order_release,
                                                                       std::memory_order_relaxed);
                    if (success) // test
                        _logger.log("tail was not updated! updated to:", new_tail);
                    else
                        _logger.log("tail was not updated but changed later! old_tail was:", old_tail);
                    continue;
                }
            }
            // non-dummy node found in the queue
            else
            {
                // data is in `old_head_next`;
                // it should be extracted before CAS to prevent reuse before extracting.
                _logger.log("extracting data from old_head_next:", old_head_next);
                result = old_head_next->obj(); // can't move, CAS might fail
                _logger.log("extracted data from old_head_next:", old_head_next);

                // can't destroy `obj` anywhere, so `T` requires to be trivially destructible
                // old_head_next->obj().~T();

                nb::TaggedPtr<Node> new_head(old_head_next.get_ptr(), old_head.get_tag() + 1);
                _logger.log("try move to new_head:", new_head);
                VTP_Q_STOP_ON_FAIL;
                // try moving new head
                if (_head.compare_exchange_weak(old_head, new_head, std::memory_order_release,
                                                std::memory_order_relaxed))
                {
                    const auto size = --_size;
                    if (size < 0)
                    {
                        _failed.store(true);
                        _logger.log("size is negative",
                                    nb::TaggedPtr<Node>(nullptr, static_cast<std::uintptr_t>(size)));
                        throw std::logic_error("negative size");
                    }
                    _logger.log("popped:", old_head);

                    // dealloc `old_head`
                    _logger.log("deallocating old_head:", old_head);
                    _node_pool.destroy(*old_head);
                    _logger.log("deallocated old_head:", old_head);

                    break;
                }
                else
                {
                    _logger.log("retry move, head changed to:", old_head);
                    continue;
                }
            }
        }

        assert(result.has_value());
        return result;
    }

public: // test
    nb::LockfreeObjectPool<Node, false, Allocator> _node_pool;

private:
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
