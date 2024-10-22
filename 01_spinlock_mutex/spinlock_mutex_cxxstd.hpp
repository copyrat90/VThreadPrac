#pragma once

#include <atomic>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace vtp::cxxstd
{

class spinlock_mutex
{
    static_assert(__cplusplus >= 202002L, "Use C++20 or higher");

private:
    std::atomic_flag _flag = ATOMIC_FLAG_INIT;

public:
    void lock()
    {
        for (;;)
        {
            if (!_flag.test_and_set(std::memory_order_acquire))
                break;

            while (_flag.test(std::memory_order_relaxed))
            {
#if defined(_WIN32)
                YieldProcessor();
#elif defined(__GNUC__)
                __builtin_ia32_pause();
#endif
            }
        }
    }

    void unlock()
    {
        _flag.clear(std::memory_order_release);
    }

    bool try_lock()
    {
        return !_flag.test(std::memory_order_relaxed) && !_flag.test_and_set(std::memory_order_acquire);
    }
};

} // namespace vtp::cxxstd
