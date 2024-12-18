#pragma once

#include <memory>
#include <type_traits>

namespace vtp
{

template <typename T, typename Allocator = std::allocator<T>>
    requires std::is_trivially_destructible_v<T>
class Queue;

}
