#include <Windows.h>
#include <process.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

using namespace std::chrono_literals;

static constexpr int PUSH_WORKERS = 3;

struct ThreadParams
{
    std::list<int>& list;
    SRWLOCK& lock;
    const bool& quit;
    HANDLE event;
};

unsigned __stdcall list_print(void* params_addr)
{
    auto& params = *reinterpret_cast<ThreadParams*>(params_addr);

    std::vector<int> list_content_copied;
    std::ostringstream oss;

    for (;;)
    {
        WaitForSingleObject(params.event, INFINITE);
        if (params.quit)
            break;

        list_content_copied.clear();
        AcquireSRWLockShared(&params.lock);
        {
            list_content_copied.reserve(params.list.size());
            std::copy(params.list.cbegin(), params.list.cend(), std::back_inserter(list_content_copied));
        }
        ReleaseSRWLockShared(&params.lock);

        oss.clear();
        oss << "list: [";
        std::copy(list_content_copied.cbegin(), list_content_copied.cend(), std::ostream_iterator<int>(oss, ", "));
        oss << "]\n";
        std::cout << oss.str();
    }

    return 0;
}

unsigned __stdcall list_pop_back(void* params_addr)
{
    auto& params = *reinterpret_cast<ThreadParams*>(params_addr);

    for (;;)
    {
        WaitForSingleObject(params.event, INFINITE);
        if (params.quit)
            break;

        AcquireSRWLockExclusive(&params.lock);
        {
            if (!params.list.empty())
                params.list.pop_back();
        }
        ReleaseSRWLockExclusive(&params.lock);
    }

    return 0;
}

unsigned __stdcall list_push_random_value_front(void* params_addr)
{
    auto& params = *reinterpret_cast<ThreadParams*>(params_addr);
    int next_number = 0;

    for (;;)
    {
        WaitForSingleObject(params.event, INFINITE);
        if (params.quit)
            break;

        AcquireSRWLockExclusive(&params.lock);
        {
            params.list.push_front(next_number);
        }
        ReleaseSRWLockExclusive(&params.lock);

        ++next_number;
    }

    return 0;
}

unsigned __stdcall list_save_to_str(void* params_addr)
{
    auto& params = *reinterpret_cast<ThreadParams*>(params_addr);

    std::vector<int> list_content_copied;
    std::ofstream f("list_thread_event.txt");

    for (;;)
    {
        WaitForSingleObject(params.event, INFINITE);
        if (params.quit)
            break;

        list_content_copied.clear();
        AcquireSRWLockShared(&params.lock);
        {
            list_content_copied.reserve(params.list.size());
            std::copy(params.list.cbegin(), params.list.cend(), std::back_inserter(list_content_copied));
        }
        ReleaseSRWLockShared(&params.lock);

        f << "list: [";
        std::copy(list_content_copied.cbegin(), list_content_copied.cend(), std::ostream_iterator<int>(f, ", "));
        f << "]\n";
    }

    return 0;
}

int main()
{
    timeBeginPeriod(1);

    std::list<int> list;
    SRWLOCK lock;
    bool quit = false;

    InitializeSRWLock(&lock);

    ThreadParams print_params{
        .list = list,
        .lock = lock,
        .quit = quit,
        .event = CreateEvent(nullptr, false, false, nullptr),
    };
    ThreadParams pop_params{
        .list = list,
        .lock = lock,
        .quit = quit,
        .event = CreateEvent(nullptr, false, false, nullptr),
    };
    ThreadParams push_params{
        .list = list,
        .lock = lock,
        .quit = quit,
        .event = CreateEvent(nullptr, false, false, nullptr),
    };
    ThreadParams save_params{
        .list = list,
        .lock = lock,
        .quit = quit,
        .event = CreateEvent(nullptr, false, false, nullptr),
    };

    static constexpr int CREATED_THREADS = 3 + PUSH_WORKERS;

    std::vector<HANDLE> threads;
    threads.reserve(CREATED_THREADS);

    threads.push_back((HANDLE)_beginthreadex(nullptr, 0, list_print, &print_params, 0, nullptr));
    threads.push_back((HANDLE)_beginthreadex(nullptr, 0, list_pop_back, &pop_params, 0, nullptr));
    threads.push_back((HANDLE)_beginthreadex(nullptr, 0, list_save_to_str, &save_params, 0, nullptr));
    for (int i = 0; i < PUSH_WORKERS; ++i)
        threads.push_back((HANDLE)_beginthreadex(nullptr, 0, list_push_random_value_front, &push_params, 0, nullptr));

    static constexpr Clock::duration PRINT_DELAY = 1s;
    static constexpr Clock::duration POP_DELAY = Clock::duration(1s) / PUSH_WORKERS;
    static constexpr Clock::duration PUSH_DELAY = 1s;

    Clock::time_point now = Clock::now();

    Clock::time_point next_print = now + PRINT_DELAY;
    Clock::time_point next_pop = now + POP_DELAY;
    Clock::time_point next_push = now + PUSH_DELAY;

    for (;;)
    {
        // 'S' 눌리면 `list_save_to_str` 스레드를 깨움
        if (1 & GetAsyncKeyState('S'))
            SetEvent(save_params.event);

        // 'Q' 눌리면 종료 절차 시작
        if (1 & GetAsyncKeyState('Q'))
            break;

        // 시간 경과에 따른 이벤트 시그널
        now = Clock::now();
        if (now >= next_print)
        {
            SetEvent(print_params.event);
            next_print += PRINT_DELAY;
        }
        if (now >= next_pop)
        {
            SetEvent(pop_params.event);
            next_pop += POP_DELAY;
        }
        if (now >= next_push)
        {
            for (int i = 0; i < PUSH_WORKERS; ++i)
                SetEvent(push_params.event);
            next_push += PUSH_DELAY;
        }

        std::this_thread::sleep_for(10ms);
    }

    // quit 세팅 후 이벤트 시그널
    quit = true;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    SetEvent(print_params.event);
    SetEvent(pop_params.event);
    for (int i = 0; i < PUSH_WORKERS; ++i)
        SetEvent(push_params.event);
    SetEvent(save_params.event);

    // 모든 스레드 종료 대기
    WaitForMultipleObjects(CREATED_THREADS, threads.data(), true, INFINITE);

    // 핸들 반환
    for (HANDLE t : threads)
        CloseHandle(t);
    CloseHandle(print_params.event);
    CloseHandle(pop_params.event);
    CloseHandle(push_params.event);
    CloseHandle(save_params.event);

    std::cout << "Goodbye!" << std::endl;

    timeEndPeriod(1);
}
