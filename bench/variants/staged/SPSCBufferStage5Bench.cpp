#include <benchmark/benchmark.h>

#include "ringbuffers/variants/staged/SPSCBufferStage5.hpp"

#include "data/BenchData.hpp"
#include "data/BenchPayload.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

using namespace ringbuffers::variants::staged;

namespace
{

template <typename T>
static void BM_SPSCBufferStage5(benchmark::State& state)
{
    constexpr std::size_t itemsPerIteration = bench::data::itemsPerIteration;

    for (auto _ : state)
    {
        const std::size_t capacity = static_cast<std::size_t>(state.range(0));
        SPSCBufferStage5<T> queue{capacity};
        
        const T payload{};

        std::thread producer{[&]
        {
            for (std::size_t i = 0; i < itemsPerIteration; ++i)
            {
                queue.push(payload);
            }

            queue.close();
        }};

        std::thread consumer{[&]
        {
            while (true)
            {
                T* value = queue.front();

                if (value != nullptr)
                {
                    queue.pop();
                }
                else if (!queue.closed())
                {
                    std::this_thread::yield();
                }
                else
                {
                    break;
                }
            }
        }};

        producer.join();
        consumer.join();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(itemsPerIteration));

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(itemsPerIteration) *
        static_cast<int64_t>(sizeof(T)));
}

} // namespace

#define REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(PayloadType) \
    BENCHMARK_TEMPLATE(BM_SPSCBufferStage5, PayloadType)  \
        ->Arg(1)                                     \
        ->Arg(4)                                     \
        ->Arg(64)                                    \
        ->Arg(256)                                   \
        ->Arg(1024)                                  \
        ->Arg(4096)                                  \
        ->Arg(16384)                                 \
        ->Arg(32768)                                 \
        ->UseRealTime()                              \
        ->MinWarmUpTime(0.5)                         \
        ->Repetitions(20)                            \
        ->DisplayAggregatesOnly(true)

REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(bench::data::BenchPayload<1>);
REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(bench::data::BenchPayload<2>);
REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(bench::data::BenchPayload<4>);
REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(bench::data::BenchPayload<8>);
REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(bench::data::BenchPayload<16>);
REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(bench::data::BenchPayload<64>);
REGISTER_SPSC_BUFFER_STAGE_5_BENCHMARK(bench::data::BenchPayload<256>);