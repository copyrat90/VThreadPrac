#include <Windows.h>
#include <process.h>

#include <iostream>

int result;

int flag[2];
int turn;

bool ready_flag;

LONG already_in_critical_section;

void lock_0()
{
    flag[0] = 1;
    turn = 0;
    for (;;)
    {
        if (!flag[1])
            break;
        if (turn != 0)
            break;
    }

    if (InterlockedExchange(&already_in_critical_section, 1))
        DebugBreak();
}

void lock_1()
{
    flag[1] = 1;
    turn = 1;
    for (;;)
    {
        if (!flag[0])
            break;
        if (turn != 1)
            break;
    }

    if (InterlockedExchange(&already_in_critical_section, 1))
        DebugBreak();
}

void unlock_0()
{
    InterlockedExchange(&already_in_critical_section, 0);

    flag[0] = 0;
}

void unlock_1()
{
    InterlockedExchange(&already_in_critical_section, 0);

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
