#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <source_location>
#include <string_view>
#include <thread>

namespace vtp
{

template <typename Param>
class MemoryLogger
{
public:
    static constexpr std::size_t MAX_LOG_ENTRIES = std::size_t(1) << 16;

    struct Entry
    {
        std::thread::id tid;
        std::string_view msg;
        Param param;
        long no;
        std::source_location loc;
    };

public:
    inline void log(std::string_view message, Param param, std::source_location loc = std::source_location::current())
    {
        const long event_idx = ++_event_index;
        auto& entry = _logs[event_idx % MAX_LOG_ENTRIES];

        entry.tid = std::this_thread::get_id();
        entry.msg = message;
        entry.param = param;
        entry.no = event_idx;
        entry.loc = loc;
    }

private:
    std::atomic<long> _event_index;
    std::array<Entry, MAX_LOG_ENTRIES> _logs;

    static_assert(decltype(_event_index)::is_always_lock_free);
};

} // namespace vtp
