#include "spinlock_mutex_cxxstd.hpp"
#if defined(_MSC_VER)
#include "spinlock_mutex_win32.hpp"
#endif

#include <iostream>
#include <thread>
#include <vector>

constexpr int COUNT = 10'000'000;

volatile int no_mutex_result = 0;
volatile int cxxstd_spinlock_mutex_result = 0;
#if defined(_MSC_VER)
volatile int win32_spinlock_mutex_result = 0;
#endif

vtp::cxxstd::spinlock_mutex cxxstd_spinlock_mutex;
#if defined(_MSC_VER)
vtp::win32::spinlock_mutex win32_spinlock_mutex;
#endif

std::atomic_flag ready_flag = ATOMIC_FLAG_INIT;

void increase_no_mutex_result_by(int count)
{
    for (int i = 0; i < count; ++i)
        ++no_mutex_result;
}

void increase_cxxstd_spinlock_mutex_result_by(int count)
{
    for (int i = 0; i < count; ++i)
    {
        cxxstd_spinlock_mutex.lock();
        ++cxxstd_spinlock_mutex_result;
        cxxstd_spinlock_mutex.unlock();
    }
}

#if defined(_MSC_VER)
void increase_win32_spinlock_mutex_result_by(int count)
{
    for (int i = 0; i < count; ++i)
    {
        win32_spinlock_mutex.lock();
        ++win32_spinlock_mutex_result;
        win32_spinlock_mutex.unlock();
    }
}
#endif

int main()
{
    int cores = static_cast<int>(std::thread::hardware_concurrency());
    if (!cores)
        cores = 2;

    std::cout << cores << " cores assumed\n";
    std::cout << cores << " cores * " << COUNT << " = " << cores * COUNT << " expected" << std::endl;

    // no mutex
    {
        std::vector<std::thread> no_mutex_threads;
        no_mutex_threads.reserve(cores);

        for (int i = 0; i < cores; ++i)
        {
            no_mutex_threads.emplace_back([]() {
                ready_flag.wait(false);
                increase_no_mutex_result_by(COUNT);
            });
        }

        // ready, set, go!
        ready_flag.test_and_set();
        ready_flag.notify_all();

        for (auto& t : no_mutex_threads)
            t.join();

        std::cout << "no_mutex_result = " << no_mutex_result << std::endl;
    }

    ready_flag.clear();

    // spinlock mutex, implemented with C++ standard threading facility
    {
        std::vector<std::thread> cxxstd_spinlock_mutex_threads;
        cxxstd_spinlock_mutex_threads.reserve(cores);

        for (int i = 0; i < cores; ++i)
        {
            cxxstd_spinlock_mutex_threads.emplace_back([]() {
                ready_flag.wait(false);
                increase_cxxstd_spinlock_mutex_result_by(COUNT);
            });
        }

        // ready, set, go!
        ready_flag.test_and_set();
        ready_flag.notify_all();

        for (auto& t : cxxstd_spinlock_mutex_threads)
            t.join();

        std::cout << "cxxstd_spinlock_mutex_result = " << cxxstd_spinlock_mutex_result << std::endl;
    }

    ready_flag.clear();

#if defined(_MSC_VER)
    // spinlock mutex, implemented with Win32 Interlocked API
    {
        std::vector<std::thread> win32_spinlock_mutex_threads;
        win32_spinlock_mutex_threads.reserve(cores);

        for (int i = 0; i < cores; ++i)
        {
            win32_spinlock_mutex_threads.emplace_back([]() {
                ready_flag.wait(false);
                increase_win32_spinlock_mutex_result_by(COUNT);
            });
        }

        // ready, set, go!
        ready_flag.test_and_set();
        ready_flag.notify_all();

        for (auto& t : win32_spinlock_mutex_threads)
            t.join();

        std::cout << "win32_spinlock_mutex_result = " << win32_spinlock_mutex_result << std::endl;
    }
#endif

    return !(cores * COUNT == cxxstd_spinlock_mutex_result
#if defined(_MSC_VER)
             && cores * COUNT == win32_spinlock_mutex_result
#endif
    );
}
