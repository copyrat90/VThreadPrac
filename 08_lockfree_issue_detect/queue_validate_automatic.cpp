#include "Queue.hpp"

#include <atomic>
#include <format>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

constexpr int PUSH_PER_THREAD = 10'000'000;

struct Item
{
    std::thread::id tid;
    int id;
};

enum class PushPopStrategy
{
    PUSH_ALL_POP_ALL,
    PING_PONG_PUSH_POP,

    TOTAL
};

struct ItemCounts
{
    std::mutex mutex;
    std::unordered_map<std::thread::id, std::deque<std::atomic<int>>> map;
} id_count_arrays;

std::optional<vtp::Queue<Item>> q;

std::atomic<bool> ready_flag;

void add_id_count(const Item& item)
{
    auto it = id_count_arrays.map.find(item.tid);
    if (it == id_count_arrays.map.end())
        throw std::logic_error("non-existing tid");

    auto& id_counts = it->second;
    ++id_counts[item.id];
}

void do_work(PushPopStrategy strategy)
{
    // prepare `id_counts` for this thread
    const std::thread::id tid = std::this_thread::get_id();
    {
        std::lock_guard<std::mutex> guard(id_count_arrays.mutex);
        auto&& [it, success] = id_count_arrays.map.try_emplace(tid, PUSH_PER_THREAD);
        if (!success)
            throw std::logic_error("duplicated tid");
    }

    // wait for start testing...
    ready_flag.wait(false);

    switch (strategy)
    {
    case PushPopStrategy::PUSH_ALL_POP_ALL:
        // push all
        for (int i = 0; i < PUSH_PER_THREAD; ++i)
            q->emplace(tid, i);
        // wait a little bit
        std::this_thread::yield();
        // pop all
        for (int i = 0; i < PUSH_PER_THREAD; ++i)
        {
            auto item = q->pop();
            if (!item.has_value())
            {
                q->_failed.store(true);
                throw std::logic_error("q was empty");
            }
            add_id_count(*item);
        }
        break;
    case PushPopStrategy::PING_PONG_PUSH_POP:
        for (int i = 0; i < PUSH_PER_THREAD; ++i)
        {
            // push
            q->emplace(tid, i);
            // wait a little bit
            std::this_thread::yield();
            // pop
            auto item = q->pop();
            if (!item.has_value())
            {
                q->_failed.store(true);
                throw std::logic_error("q was empty");
            }
            add_id_count(*item);
        }
        break;

    default:
        throw std::logic_error(std::format("Invalid strategy = {}", static_cast<int>(strategy)));
    }
}

int main()
{
    q.emplace();
    std::ostringstream err_oss;
    q->_node_pool.set_err_stream(&err_oss);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution dist(0, static_cast<int>(PushPopStrategy::TOTAL) - 1);

    const unsigned cores = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 2;
    std::cout << "Preparing " << cores << " concurrent threads...\n";
    std::vector<std::thread> threads;
    threads.reserve(cores);

    for (unsigned i = 0; i < cores; ++i)
        threads.emplace_back(do_work, static_cast<PushPopStrategy>(dist(rng)));

    // ready, set, go!
    ready_flag.store(true);
    ready_flag.notify_all();

    for (auto& t : threads)
        t.join();

    for (const auto& [tid, id_counts] : id_count_arrays.map)
    {
        for (const auto& id_count : id_counts)
        {
            if (id_count.load() != 1)
            {
                std::cout << "id_count was not 1" << std::endl;
                throw std::logic_error("id_count was not 1");
            }
        }
    }

    q.reset();
    std::string err_str = err_oss.str();
    if (!err_str.empty())
    {
        std::cout << err_str << std::endl;
        throw std::logic_error("internal obj pool error");
    }

    std::cout << "All is well!" << std::endl;
}
