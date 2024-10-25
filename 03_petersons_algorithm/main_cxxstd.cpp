#include <atomic>
#include <format>
#include <iostream>
#include <sstream>
#include <thread>

int result;

static_assert(std::atomic<bool>::is_always_lock_free);

std::atomic<bool> flag[2];
std::atomic<bool> turn;

std::atomic<bool> ready_flag;

std::atomic<int> already_in_critical_section;

struct flags_buffer
{
    int flag[2];
    int turn;
};

std::ostream& operator<<(std::ostream& os, const flags_buffer& buf)
{
    os << "flag[0]: " << buf.flag[0] << "\nflag[1]: " << buf.flag[1] << "\nturn: " << buf.turn << std::endl;
    return os;
}

flags_buffer error_flags_0, error_flags_1;

void fence_print_error_flags()
{
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::ostringstream oss;
    oss << "[thread #0]\n" << error_flags_0 << "[thread #1]\n" << error_flags_1 << "\n" << std::endl;
    std::cout << oss.str();
}

void lock_0()
{
    flags_buffer flags;

    flag[0].store(1, std::memory_order_relaxed);
    flags.flag[0] = flag[0].load(std::memory_order_relaxed);
    turn.store(0, std::memory_order_relaxed);
    flags.turn = turn.load(std::memory_order_relaxed);
    for (;;)
    {
        if (!(flags.flag[1] = flag[1].load(std::memory_order_relaxed)))
            break;
        if ((flags.turn = turn.load(std::memory_order_relaxed)) != 0)
            break;
    }

    error_flags_0 = flags;
    if (1 != ++already_in_critical_section)
        fence_print_error_flags();
}

void lock_1()
{
    flags_buffer flags;

    flag[1].store(1, std::memory_order_relaxed);
    flags.flag[1] = flag[1].load(std::memory_order_relaxed);
    turn.store(1, std::memory_order_relaxed);
    flags.turn = turn.load(std::memory_order_relaxed);
    for (;;)
    {
        if (!(flags.flag[0] = flag[0].load(std::memory_order_relaxed)))
            break;
        if ((flags.turn = turn.load(std::memory_order_relaxed)) != 1)
            break;
    }

    error_flags_1 = flags;
    if (1 != ++already_in_critical_section)
        fence_print_error_flags();
}

void unlock_0()
{
    if (0 != --already_in_critical_section)
        fence_print_error_flags();

    flag[0].store(0, std::memory_order_release);
}

void unlock_1()
{
    if (0 != --already_in_critical_section)
        fence_print_error_flags();

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
}
