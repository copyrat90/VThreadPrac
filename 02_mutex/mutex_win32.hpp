#pragma once

#include <Windows.h>

namespace vtp::win32
{

class mutex
{
private:
    LONG _flag = 0;

public:
    void lock()
    {
        LONG old = 1;
        while (InterlockedExchange(&_flag, 1))
            WaitOnAddress(&_flag, &old, sizeof(_flag), INFINITE);
    }

    void unlock()
    {
        MemoryBarrier();
        _flag = 0;
        MemoryBarrier();
        WakeByAddressSingle(&_flag);
    }
};

} // namespace vtp::win32
