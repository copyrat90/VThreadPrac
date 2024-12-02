#include "Stack.hpp"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

constexpr std::size_t ITEMS_PER_THREAD = std::size_t(1) << 20;

std::atomic<bool> ready_flag;
std::atomic<bool> stop_flag;
static_assert(std::atomic<bool>::is_always_lock_free);

vtp::Stack<std::size_t> st;

void pop_worker()
{
    ready_flag.wait(false);

    for (std::size_t i = 0; i < ITEMS_PER_THREAD; ++i)
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

int main()
{
    unsigned cores = 2;

    for (std::size_t i = 0; i < cores * ITEMS_PER_THREAD; ++i)
        st.push(i);

    std::cout << "items in stack: " << cores * ITEMS_PER_THREAD << "\n";

    std::cout << "testing with " << cores << " cores...\n";

    std::vector<std::thread> threads;
    threads.reserve(cores);

    for (unsigned i = 0; i < cores; ++i)
        threads.emplace_back(pop_worker);

    ready_flag.store(true);
    ready_flag.notify_all();

    for (auto& thread : threads)
        thread.join();
}
