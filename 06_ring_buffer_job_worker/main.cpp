#include <NetBuff/RingByteBuffer.hpp>

#define NOMINMAX
#include <Windows.h>
#include <process.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <list>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

void msg_queue_exception_exit()
{
    std::cout << "Something went wrong\n";
    // TODO: 정상적인 종료 절차 밟기
    throw std::logic_error("Something went wrong");
}

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

static constexpr int WORKER_THREADS = 3;
static constexpr Clock::duration MAIN_LOOP_WAIT_DURATION = 50ms;

static constexpr int RING_BUFFER_SIZE = 50'000;

enum class JobMsgType : std::uint8_t
{
    LIST_PUSH_BACK,
    LIST_POP_FRONT,
    LIST_SORT,
    LIST_FIND,
    LIST_PRINT,
    QUIT,
};

struct JobMsgHeader
{
    JobMsgType type;
    std::uint8_t payload_length;
};

struct List
{
    std::list<std::string> list;
    SRWLOCK lock;
} list;

struct MsgQueue
{
    nb::RingByteBuffer<> queue = nb::RingByteBuffer<>(RING_BUFFER_SIZE);
    SRWLOCK lock;
} msg_queue;

HANDLE event;

unsigned __stdcall worker(void*)
{
    const DWORD thread_id = GetCurrentThreadId();

    std::ostringstream oss;

    for (;;)
    {
        JobMsgHeader header;
        std::string payload_str;

        AcquireSRWLockExclusive(&msg_queue.lock);
        {
            // 일감 없으면
            while (msg_queue.queue.empty())
            {
                // 메시지큐 락 해제 후 이벤트 대기
                ReleaseSRWLockExclusive(&msg_queue.lock);
                WaitForSingleObject(event, INFINITE);
                // 다시 락 얻고 재시도
                AcquireSRWLockExclusive(&msg_queue.lock);
            }

            // 일감이 있고, 내가 메시지큐 락을 소유하니, 내가 꺼내 처리할 준비
            if (!msg_queue.queue.try_read(&header, sizeof(header)))
                msg_queue_exception_exit();
            if (JobMsgType::LIST_PUSH_BACK == header.type || JobMsgType::LIST_FIND == header.type)
            {
                payload_str.resize(header.payload_length);
                if (!msg_queue.queue.try_read(payload_str.data(), header.payload_length))
                    msg_queue_exception_exit();
            }
        }
        ReleaseSRWLockExclusive(&msg_queue.lock);

        // 종료 메시지 처리
        if (JobMsgType::QUIT == header.type)
        {
            SetEvent(event);
            break;
        }

        // const bool is_shared_lock = (JobMsgType::LIST_FIND == header.type || JobMsgType::LIST_PRINT == header.type);
        // auto lock_func = (is_shared_lock) ? AcquireSRWLockShared : AcquireSRWLockExclusive;
        // auto unlock_func = (is_shared_lock) ? ReleaseSRWLockShared : ReleaseSRWLockExclusive;

        switch (header.type)
        {
        case JobMsgType::LIST_PUSH_BACK:
            AcquireSRWLockExclusive(&list.lock);
            {
                list.list.push_back(std::move(payload_str));
            }
            ReleaseSRWLockExclusive(&list.lock);
            payload_str.clear();
            break;
        case JobMsgType::LIST_POP_FRONT:
            AcquireSRWLockExclusive(&list.lock);
            {
                if (!list.list.empty())
                    list.list.pop_front();
            }
            ReleaseSRWLockExclusive(&list.lock);
            break;
        case JobMsgType::LIST_SORT:
            AcquireSRWLockExclusive(&list.lock);
            {
                list.list.sort();
            }
            ReleaseSRWLockExclusive(&list.lock);
            break;
        case JobMsgType::LIST_FIND: {
            bool found;
            AcquireSRWLockShared(&list.lock);
            {
                auto it = std::find(list.list.cbegin(), list.list.cend(), payload_str);
                found = (it != list.list.cend());
            }
            ReleaseSRWLockShared(&list.lock);
            if (found)
                std::cout << std::format("[TID #{}] \"{}\" found in list\n", thread_id, payload_str);
            else
                std::cout << std::format("[TID #{}] \"{}\" not found in list\n", thread_id, payload_str);
            break;
        }
        case JobMsgType::LIST_PRINT:
            oss.clear();
            oss << "[TID #" << thread_id << "] list: [";
            AcquireSRWLockShared(&list.lock);
            {
                std::copy(list.list.cbegin(), list.list.cend(), std::ostream_iterator<std::string>(oss, ", "));
            }
            ReleaseSRWLockShared(&list.lock);
            oss << "]\n";
            std::cout << oss.str();
            break;
        default:
            std::cout << "Should not reach here\n";
            std::exit(-1); // TODO: `std::exit(-1)` 안 쓰고 종료 절차 밟기
        }
    }

    std::cout << std::format("Worker #{} returns\n", thread_id);
    return 0;
}

int main()
{
    timeBeginPeriod(1);

    event = CreateEvent(nullptr, false, false, nullptr);
    InitializeSRWLock(&list.lock);
    InitializeSRWLock(&msg_queue.lock);

    std::vector<HANDLE> threads;
    threads.reserve(WORKER_THREADS);
    for (int i = 0; i < WORKER_THREADS; ++i)
        threads.push_back((HANDLE)_beginthreadex(nullptr, 0, worker, nullptr, 0, nullptr));

    std::mt19937 rng(std::random_device{}());

    std::uniform_int_distribution<int> random_job(0, (int)JobMsgType::QUIT - 1);
    std::uniform_int_distribution<int> random_length(1, 7);
    std::uniform_int_distribution<int> random_char('a', 'z');

    std::string str;
    str.reserve(std::numeric_limits<decltype(JobMsgHeader::payload_length)>::max());

    auto update_random_str = [&str, &rng, &random_char](int length) {
        str.clear();
        for (int i = 0; i < length; ++i)
            str.push_back(static_cast<char>(random_char(rng)));
    };

    auto now = Clock::now();
    auto next_sleep = now + MAIN_LOOP_WAIT_DURATION;

    JobMsgHeader header;

    for (;;)
    {
        // 'Q' 눌리면, 종료 절차 시작
        if (1 & GetAsyncKeyState('Q'))
            break;

        // 일감 생성
        header.type = static_cast<JobMsgType>(random_job(rng));
        header.payload_length = 0;

        if (JobMsgType::LIST_PUSH_BACK == header.type || JobMsgType::LIST_FIND == header.type)
        {
            header.payload_length = static_cast<decltype(JobMsgHeader::payload_length)>(random_length(rng));
            update_random_str(header.payload_length);
        }

        AcquireSRWLockExclusive(&msg_queue.lock);
        {
            if (!msg_queue.queue.try_write(&header, sizeof(header)))
                msg_queue_exception_exit();

            if (header.payload_length)
                if (!msg_queue.queue.try_write(str.data(), header.payload_length))
                    msg_queue_exception_exit();
        }
        ReleaseSRWLockExclusive(&msg_queue.lock);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        SetEvent(event);

        // sleep
        now = Clock::now();
        if (next_sleep > now)
            std::this_thread::sleep_for(next_sleep - now);
        next_sleep += MAIN_LOOP_WAIT_DURATION;
    }

    // 종료 메시지 enqueue
    header.type = JobMsgType::QUIT;
    header.payload_length = 0;

    AcquireSRWLockExclusive(&msg_queue.lock);
    {
        for (int i = 0; i < WORKER_THREADS; ++i)
            if (!msg_queue.queue.try_write(&header, sizeof(header)))
                msg_queue_exception_exit();
    }
    ReleaseSRWLockExclusive(&msg_queue.lock);

    std::atomic_thread_fence(std::memory_order_seq_cst);

    // 워커 스레드 깨우기
    for (int i = 0; i < WORKER_THREADS; ++i)
        SetEvent(event);

    WaitForMultipleObjects(WORKER_THREADS, threads.data(), true, INFINITE);

    for (HANDLE h : threads)
        CloseHandle(h);
    CloseHandle(event);

    std::cout << "Goodbye!" << std::endl;

    timeEndPeriod(1);
}
