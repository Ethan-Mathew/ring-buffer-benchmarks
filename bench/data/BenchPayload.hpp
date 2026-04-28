#pragma once

#include <cstddef>

namespace bench::data
{

template <std::size_t N>
struct BenchPayload
{
    std::byte payload[N];
};

} // namespace bench::data