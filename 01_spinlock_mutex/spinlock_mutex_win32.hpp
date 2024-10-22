#pragma once

#include <Windows.h>

namespace vtp::win32
{

class spinlock_mutex
{
private:
    LONG _flag = 0;

public:
    void lock()
    {
        while (InterlockedExchange(&_flag, 1))
            ;
    }

    void unlock()
    {
        // We need a fence to prevent the code inside critical section from going down this `unlock()` code below
        MemoryBarrier();

        // Simple write to aligned 32-bit variable is atomic
        // https://learn.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
        _flag = 0;
    }
};

} // namespace vtp::win32
