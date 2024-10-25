#include <Windows.h>
#include <process.h>

#include <array>
#include <atomic>
#include <format>
#include <iostream>
#include <random>

int g_Data = 0;
int g_Connect = 0;
bool g_Shutdown = false;

SRWLOCK g_Connect_lock;

unsigned __stdcall acceptor(void*)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(100, 1000);
    bool shutdown_false = false;

    for (;;)
    {
        AcquireSRWLockExclusive(&g_Connect_lock);
        ++g_Connect;
        ReleaseSRWLockExclusive(&g_Connect_lock);

        const auto sleep_time = dist(rng);
        WaitOnAddress(&g_Shutdown, &shutdown_false, sizeof(g_Shutdown), sleep_time);

        // g_Shutdown 에 새로운 값이 쓰였다면, 반영
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (g_Shutdown)
            break;
    }

    std::cout << "acceptor() returns\n";
    return 0;
}

unsigned __stdcall disconnector(void*)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(100, 1000);
    bool shutdown_false = false;

    for (;;)
    {
        AcquireSRWLockExclusive(&g_Connect_lock);
        if (g_Connect > 0)
            --g_Connect;
        ReleaseSRWLockExclusive(&g_Connect_lock);

        const auto sleep_time = dist(rng);
        WaitOnAddress(&g_Shutdown, &shutdown_false, sizeof(g_Shutdown), sleep_time);

        // g_Shutdown 에 새로운 값이 쓰였다면, 반영
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (g_Shutdown)
            break;
    }

    std::cout << "disconnector() returns\n";
    return 0;
}

unsigned __stdcall updater(void*)
{
    constexpr int SLEEP_TIME = 10;
    bool shutdown_false = false;

    for (;;)
    {
        const LONG data = InterlockedIncrement((LONG*)(&g_Data));
        if (data % 1000 == 0)
            std::cout << std::format("g_Data: {}\n", data);

        WaitOnAddress(&g_Shutdown, &shutdown_false, sizeof(g_Shutdown), SLEEP_TIME);

        // g_Shutdown 에 새로운 값이 쓰였다면, 반영
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (g_Shutdown)
            break;
    }

    std::cout << "updater() returns\n";
    return 0;
}

int main()
{
    timeBeginPeriod(1);

    InitializeSRWLock(&g_Connect_lock);

    std::cout << "Starting threads...\n";

    std::array<HANDLE, 5> threads = {
        (HANDLE)_beginthreadex(nullptr, 0, acceptor, nullptr, 0, nullptr),
        (HANDLE)_beginthreadex(nullptr, 0, disconnector, nullptr, 0, nullptr),
        (HANDLE)_beginthreadex(nullptr, 0, updater, nullptr, 0, nullptr),
        (HANDLE)_beginthreadex(nullptr, 0, updater, nullptr, 0, nullptr),
        (HANDLE)_beginthreadex(nullptr, 0, updater, nullptr, 0, nullptr),
    };

    for (int i = 0; i < 20; ++i)
    {
        Sleep(1000);

        AcquireSRWLockShared(&g_Connect_lock);
        const int connect = g_Connect;
        ReleaseSRWLockShared(&g_Connect_lock);

        std::cout << std::format("g_Connect: {}\n", connect);
    }

    InterlockedExchange8((char*)&g_Shutdown, (char)true);
    WakeByAddressAll(&g_Shutdown);

    WaitForMultipleObjects(5, threads.data(), true, INFINITE);

    for (HANDLE hnd : threads)
        CloseHandle(hnd);

    timeEndPeriod(1);

    std::cout << "main() returns\n";
}
