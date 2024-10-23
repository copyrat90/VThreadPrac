#pragma once

#include <atomic>

namespace vtp::cxxstd
{

class mutex
{
    static_assert(__cplusplus >= 202002L, "Use C++20 or higher");

private:
    std::atomic_flag _flag = ATOMIC_FLAG_INIT;

public:
    void lock()
    {
        while (_flag.test_and_set(std::memory_order_acquire))
            _flag.wait(true, std::memory_order_relaxed);
    }

    void unlock()
    {
        _flag.clear(std::memory_order_release);
        _flag.notify_one();
    }
};

} // namespace vtp::cxxstd
