#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string_view>
#include <thread>

namespace vtp
{

template <typename Param, std::size_t MaxEntries>
class MemoryLogger
{
public:
    struct Entry
    {
        std::thread::id tid;
        std::string_view msg;
        Param param;
        std::size_t no;
        std::source_location loc;
    };

public:
    void log(std::string_view message, Param param, std::source_location loc = std::source_location::current())
    {
        const auto event_idx = _event_index++;
        auto& entry = _logs[event_idx % MaxEntries];

        entry.tid = std::this_thread::get_id();
        entry.msg = message;
        entry.param = param;
        entry.no = event_idx;
        entry.loc = loc;
    }

private:
    std::atomic<std::size_t> _event_index;
    std::array<Entry, MaxEntries> _logs;

    static_assert(decltype(_event_index)::is_always_lock_free);
};

} // namespace vtp
