#include <gtest/gtest.h>

#include <ringbuffers/SPSCBuffer.hpp>

#include <atomic>
#include <cstddef>
#include <thread>

using namespace ringbuffers;

static constexpr std::size_t bufferSize = 1024;
static constexpr std::size_t stressTestSize = 1'000'000;

TEST(SPSCBufferStressTest, PreservesOrderingUnderLoad)
{
    SPSCBuffer<int> buffer{bufferSize};

    std::atomic<bool> observedOutOfOrder = false;
    std::atomic<std::size_t> consumedCount = 0;

    std::thread producer{[&]
    {
        for (std::size_t i = 0; i < stressTestSize; ++i)
        {
            buffer.push(static_cast<int>(i));
        }

        buffer.close();
    }};

    std::thread consumer{[&]
    {
        std::size_t expected = 0;

        while (true)
        {
            auto* val = buffer.front();

            if (val != nullptr)
            {
                if (*val != static_cast<int>(expected))
                {
                    observedOutOfOrder.store(true, std::memory_order_relaxed);
                }

                buffer.pop();
                ++expected;
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            }
            else if (buffer.closed())
            {
                break;
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }};

    producer.join();
    consumer.join();

    EXPECT_FALSE(observedOutOfOrder.load(std::memory_order_relaxed));
    EXPECT_EQ(consumedCount.load(std::memory_order_relaxed), stressTestSize);
}