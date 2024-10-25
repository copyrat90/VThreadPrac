#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>

int result;

std::atomic<int> err_both_cs_count;

static_assert(std::atomic<bool>::is_always_lock_free);

std::atomic<bool> flag[2];
std::atomic<bool> turn;

std::atomic<bool> ready_flag;

std::atomic<bool> already_in_critical_section;

void lock_0()
{
    flag[0].store(1, std::memory_order_relaxed);
    turn.store(0, std::memory_order_relaxed);
    for (;;)
    {
        if (!flag[1].load(std::memory_order_relaxed))
            break;
        if (turn.load(std::memory_order_relaxed) != 0)
            break;
    }

    if (already_in_critical_section.exchange(1))
    {
        ++err_both_cs_count;
        throw std::logic_error("Both threads are in critical section");
    }
}

void lock_1()
{
    flag[1].store(1, std::memory_order_relaxed);
    turn.store(1, std::memory_order_relaxed);
    for (;;)
    {
        if (!flag[0].load(std::memory_order_relaxed))
            break;
        if (turn.load(std::memory_order_relaxed) != 1)
            break;
    }

    if (already_in_critical_section.exchange(1))
    {
        ++err_both_cs_count;
        throw std::logic_error("Both threads are in critical section");
    }
}

void unlock_0()
{
    already_in_critical_section = 0;

    flag[0].store(0, std::memory_order_release);
}

void unlock_1()
{
    already_in_critical_section = 0;

    flag[1].store(0, std::memory_order_release);
}

void increase_result_0(int loop_count)
{
    // get ready...
    ready_flag.wait(false);
    // GO!!

    for (int i = 0; i < loop_count; ++i)
    {
        lock_0();

        ++result;

        unlock_0();
    }
}

void increase_result_1(int loop_count)
{
    // get ready...
    ready_flag.wait(false);
    // GO!!

    for (int i = 0; i < loop_count; ++i)
    {
        lock_1();

        ++result;

        unlock_1();
    }
}

int main()
{
    static constexpr int LOOP_COUNT = 100'000'000;

    std::cout << "expected: " << 2 * LOOP_COUNT << std::endl;

    std::thread thread_0(increase_result_0, LOOP_COUNT);
    std::thread thread_1(increase_result_1, LOOP_COUNT);

    // ready, set, GO!!
    ready_flag = true;
    ready_flag.notify_all();

    thread_0.join();
    thread_1.join();

    std::cout << "result: " << result << std::endl;
    std::cout << "err_both_cs_count: " << err_both_cs_count << std::endl;
}
