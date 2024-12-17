#include "Queue.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#define VTP_Q_ISSUE_STOP_ON_FAIL \
    do \
    { \
        if (q._failed.load()) \
            throw std::logic_error("stopped"); \
    } while (false);

constexpr int LOOP_COUNT = 2;

unsigned g_cores;

vtp::Queue<int> q;

void worker()
{
    for (;;)
    {
        for (int i = 0; i < LOOP_COUNT; ++i)
        {
            VTP_Q_ISSUE_STOP_ON_FAIL;
            const std::size_t size = q.push(i);
            if (size > std::size_t(LOOP_COUNT) * g_cores)
            {
                q._failed.store(true);
                throw std::logic_error("too many items in q");
            }
        }
        for (int i = 0; i < LOOP_COUNT; ++i)
        {
            VTP_Q_ISSUE_STOP_ON_FAIL;
            if (!q.pop().has_value())
            {
                q._failed.store(true);
                throw std::logic_error("no item in q");
            }
        }
    }
}

int main()
{
    g_cores = 2;

    std::cout << "testing with " << g_cores << " cores...\n";

    std::vector<std::thread> threads;
    threads.reserve(g_cores);

    for (unsigned i = 0; i < g_cores; ++i)
        threads.emplace_back(worker);

    for (auto& thread : threads)
        thread.join();
}
