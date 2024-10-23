#include "mutex_cxxstd.hpp"
#if defined(_MSC_VER)
#include "mutex_win32.hpp"
#endif

#include <iostream>
#include <thread>
#include <vector>

constexpr int COUNT = 10'000'000;

volatile int cxxstd_mutex_result = 0;
#if defined(_MSC_VER)
volatile int win32_mutex_result = 0;
#endif

vtp::cxxstd::mutex cxxstd_mutex;
#if defined(_MSC_VER)
vtp::win32::mutex win32_mutex;
#endif

std::atomic_flag ready_flag = ATOMIC_FLAG_INIT;

void increase_cxxstd_mutex_result_by(int count)
{
    for (int i=0;i<count;++i)
    {
        cxxstd_mutex.lock();
        ++cxxstd_mutex_result;
        cxxstd_mutex.unlock();
    }
}

#if defined(_MSC_VER)
void increase_win32_mutex_result_by(int count)
{
    for (int i = 0; i < count; ++i)
    {
        win32_mutex.lock();
        ++win32_mutex_result;
        win32_mutex.unlock();
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

    // mutex, implemented with C++ standard threading facility
    {
        std::vector<std::thread> cxxstd_mutex_threads;
        cxxstd_mutex_threads.reserve(cores);

        for (int i = 0; i < cores; ++i)
        {
            cxxstd_mutex_threads.emplace_back([]() {
                ready_flag.wait(false);
                increase_cxxstd_mutex_result_by(COUNT);
            });
        }

        // ready, set, go!
        ready_flag.test_and_set();
        ready_flag.notify_all();

        for (auto& t : cxxstd_mutex_threads)
            t.join();

        std::cout << "cxxstd_mutex_result = " << cxxstd_mutex_result << std::endl;
    }

    ready_flag.clear();

#if defined(_MSC_VER)
    // mutex, implemented with Win32 Interlocked API
    {
        std::vector<std::thread> win32_mutex_threads;
        win32_mutex_threads.reserve(cores);

        for (int i = 0; i < cores; ++i)
        {
            win32_mutex_threads.emplace_back([]() {
                ready_flag.wait(false);
                increase_win32_mutex_result_by(COUNT);
            });
        }

        // ready, set, go!
        ready_flag.test_and_set();
        ready_flag.notify_all();

        for (auto& t : win32_mutex_threads)
            t.join();

        std::cout << "win32_mutex_result = " << win32_mutex_result << std::endl;
    }
#endif

    return !(cores * COUNT == cxxstd_mutex_result
#if defined(_MSC_VER)
             && cores * COUNT == win32_mutex_result
#endif
    );
}
