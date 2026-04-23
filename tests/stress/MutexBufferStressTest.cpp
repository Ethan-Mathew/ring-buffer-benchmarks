#include <gtest/gtest.h>

#include <ringbuffers/MutexBuffer.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

using namespace ringbuffers;

static constexpr std::size_t bufferSize = 1024;
static constexpr std::size_t stressTestSize = 1'000'000;

class MutexBufferTest : public testing::Test
{
protected:
    MutexBufferTest()
        : mutexBuffer_{bufferSize}
    {
    }

    MutexBuffer<int> mutexBuffer_;
};

TEST_F(MutexBufferTest, SPSCBehaviourStressTest)
{
    std::thread producer{[&]{
        for (int i = 0; i < stressTestSize; i++)
        {
            mutexBuffer_.push(i);
        }
    }};

    std::thread consumer{[&]{
        int expected = 0;

        while (true)
        {
            auto val = mutexBuffer_.front();
            
            if (val)
            {
                EXPECT_EQ(*val, expected);

                mutexBuffer_.pop();

                expected++;
            }
            else if (mutexBuffer_.closed())
            {
                break;
            }
        }
    }};
    
    producer.join();

    mutexBuffer_.close();

    consumer.join();
}

TEST_F(MutexBufferTest, MPMCBehaviourStressTest)
{
    const int numProducers = 8;
    const int numConsumers = 8;

    std::vector<std::thread> consumers;

    const int itemsPerProducer = stressTestSize / numProducers;

    std::atomic<int> numProduced = 0;
    std::atomic<int> numConsumed = 0;

    
    std::vector<std::thread> producers;

    for (std::size_t i = 0; i < numProducers; i++)
    {
        producers.emplace_back([&]{
            for (int i = 0; i < itemsPerProducer; i++)
            {
                mutexBuffer_.push(i);
                numProduced++;
            }
        });
    }

    for (std::size_t i = 0; i < numConsumers; i++)
    {
        consumers.emplace_back([&]{
            while (true)
            {
                auto val = mutexBuffer_.try_pop();
                
                if (val)
                {
                    numConsumed++;
                }
                else if (mutexBuffer_.closed())
                {
                    break;
                }
            }
        });
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    mutexBuffer_.close();

    for (auto& consumer : consumers)
    {
        consumer.join();
    }

    ASSERT_EQ(numProduced, stressTestSize);
    ASSERT_EQ(numConsumed, stressTestSize);
}