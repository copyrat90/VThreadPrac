#include <Windows.h>
#include <process.h>

#include <iostream>

int result;

int flag[2];
int turn;

bool ready_flag;

LONG already_in_critical_section;

struct flags_buffer
{
    int flag[2];
    int turn;
};

flags_buffer error_flags_0, error_flags_1;

void lock_0()
{
    flags_buffer flags;

    flags.flag[0] = flag[0] = 1;
    flags.turn = turn = 0;
    for (;;)
    {
        if (!(flags.flag[1] = flag[1]))
            break;
        if ((flags.turn = turn) != 0)
            break;
    }

    error_flags_0 = flags;
    if (1 != InterlockedIncrement(&already_in_critical_section))
        DebugBreak();
}

void lock_1()
{
    flags_buffer flags;

    flags.flag[1] = flag[1] = 1;
    flags.turn = turn = 1;
    for (;;)
    {
        if (!(flags.flag[0] = flag[0]))
            break;
        if ((flags.turn = turn) != 1)
            break;
    }

    error_flags_1 = flags;
    if (1 != InterlockedIncrement(&already_in_critical_section))
        DebugBreak();
}

void unlock_0()
{
    if (0 != InterlockedDecrement(&already_in_critical_section))
        DebugBreak();

    flag[0] = 0;
}

void unlock_1()
{
    if (0 != InterlockedDecrement(&already_in_critical_section))
        DebugBreak();

    flag[1] = 0;
}

unsigned __stdcall increase_result_0(void* loop_count)
{
    // get ready...
    bool not_ready = false;
    WaitOnAddress(&ready_flag, &not_ready, sizeof(ready_flag), INFINITE);
    // GO!!

    const int count = *(int*)loop_count;
    for (int i = 0; i < count; ++i)
    {
        lock_0();

        ++result;

        unlock_0();
    }

    return 0;
}

unsigned __stdcall increase_result_1(void* loop_count)
{
    // get ready...
    bool not_ready = false;
    WaitOnAddress(&ready_flag, &not_ready, sizeof(ready_flag), INFINITE);
    // GO!!

    const int count = *(int*)loop_count;
    for (int i = 0; i < count; ++i)
    {
        lock_1();

        ++result;

        unlock_1();
    }

    return 0;
}

int main()
{
    int LOOP_COUNT = 10'000'000;

    std::cout << "expected: " << 2 * LOOP_COUNT << std::endl;

    auto thread_0 = (HANDLE)_beginthreadex(nullptr, 0, increase_result_0, &LOOP_COUNT, 0, nullptr);
    if (!thread_0)
    {
        std::cout << "thread #0 creation failed!" << std::endl;
        return 1;
    }

    auto thread_1 = (HANDLE)_beginthreadex(nullptr, 0, increase_result_1, &LOOP_COUNT, 0, nullptr);
    if (!thread_1)
    {
        std::cout << "thread #1 creation failed!" << std::endl;
        return 1;
    }

    // ready, set, GO!!
    ready_flag = true;
    WakeByAddressAll(&ready_flag);

    WaitForSingleObject(thread_0, INFINITE);
    WaitForSingleObject(thread_1, INFINITE);

    std::cout << "result: " << result << std::endl;

    CloseHandle(thread_0);
    CloseHandle(thread_1);
}
