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
#include <vector>

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
} g_id_count_arrays;

std::optional<vtp::Queue<Item>> g_queue;

std::atomic<bool> g_ready_flag;

void add_id_count(std::unordered_map<std::thread::id, std::vector<int>>& id_counts_map, const Item& item)
{
    auto it = id_counts_map.find(item.tid);
    if (it == id_counts_map.end())
    {
        auto&& [new_it, success] = id_counts_map.try_emplace(item.tid, PUSH_PER_THREAD);
        if (!success)
            throw std::logic_error("duplicated tid");
        it = new_it;
    }

    auto& id_counts = it->second;
    ++id_counts[item.id];
}

void do_work(PushPopStrategy strategy, const unsigned cores)
{
    const std::thread::id tid = std::this_thread::get_id();

    // prepare `id_counts` for this thread
    std::unordered_map<std::thread::id, std::vector<int>> id_counts_map;
    id_counts_map.reserve(cores);

    // wait for start testing...
    g_ready_flag.wait(false);

    switch (strategy)
    {
    case PushPopStrategy::PUSH_ALL_POP_ALL:
        // push all
        for (int i = 0; i < PUSH_PER_THREAD; ++i)
            g_queue->emplace(tid, i);
        // wait a little bit
        std::this_thread::yield();
        // pop all
        for (int i = 0; i < PUSH_PER_THREAD; ++i)
        {
            auto item = g_queue->pop();
            if (!item.has_value())
            {
                g_queue->_failed.store(true);
                throw std::logic_error("g_queue was empty");
            }
            add_id_count(id_counts_map, *item);
        }
        break;
    case PushPopStrategy::PING_PONG_PUSH_POP:
        for (int i = 0; i < PUSH_PER_THREAD; ++i)
        {
            // push
            g_queue->emplace(tid, i);
            // wait a little bit
            std::this_thread::yield();
            // pop
            auto item = g_queue->pop();
            if (!item.has_value())
            {
                g_queue->_failed.store(true);
                throw std::logic_error("g_queue was empty");
            }
            add_id_count(id_counts_map, *item);
        }
        break;

    default:
        throw std::logic_error(std::format("Invalid strategy = {}", static_cast<int>(strategy)));
    }

    // sum results to global `g_id_count_arrays`
    for (const auto& [item_tid, id_counts] : id_counts_map)
    {
        g_id_count_arrays.mutex.lock();
        auto&& [it, success] = g_id_count_arrays.map.try_emplace(item_tid, PUSH_PER_THREAD);
        auto& global_id_counts = it->second;
        g_id_count_arrays.mutex.unlock();

        for (int i = 0; i < PUSH_PER_THREAD; ++i)
            global_id_counts[i].fetch_add(id_counts[i]);
    }
}

int main()
{
    g_queue.emplace();
    std::ostringstream err_oss;
    g_queue->_node_pool.set_err_stream(&err_oss);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution dist(0, static_cast<int>(PushPopStrategy::TOTAL) - 1);

    const unsigned cores = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 2;
    std::cout << "Preparing " << cores << " concurrent threads...\n";
    g_id_count_arrays.map.reserve(cores);
    std::vector<std::thread> threads;
    threads.reserve(cores);

    for (unsigned i = 0; i < cores; ++i)
        threads.emplace_back(do_work, static_cast<PushPopStrategy>(dist(rng)), cores);

    // ready, set, go!
    g_ready_flag.store(true);
    g_ready_flag.notify_all();

    for (auto& t : threads)
        t.join();

    for (const auto& [tid, id_counts] : g_id_count_arrays.map)
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

    g_queue.reset();
    std::string err_str = err_oss.str();
    if (!err_str.empty())
    {
        std::cout << err_str << std::endl;
        throw std::logic_error("internal obj pool error");
    }

    std::cout << "All is well!" << std::endl;
}
