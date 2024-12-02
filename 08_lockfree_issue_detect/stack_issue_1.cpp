#include "Stack.hpp"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

constexpr int LOOP_COUNT = 2;

std::atomic<bool> stop_flag;
static_assert(decltype(stop_flag)::is_always_lock_free);

vtp::Stack<int> st;

void push_worker()
{
    while (true)
    {
        for (int i = 0; i < LOOP_COUNT; ++i)
        {
            if (stop_flag.load())
                throw std::logic_error("");

            st.push(i);
        }
        for (int i = 0; i < LOOP_COUNT; ++i)
        {
            if (stop_flag.load())
                throw std::logic_error("");

            if (!st.pop().has_value())
            {
                stop_flag.store(true);
                throw std::logic_error("");
            }
        }
    }
}

int main()
{
    unsigned cores = 2;

    std::cout << "testing with " << cores << " cores...\n";

    std::vector<std::thread> threads;
    threads.reserve(cores);

    for (unsigned i = 0; i < cores; ++i)
        threads.emplace_back(push_worker);

    for (auto& thread : threads)
        thread.join();
}
