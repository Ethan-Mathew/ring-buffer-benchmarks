#include <gtest/gtest.h>

#include <ringbuffers/MutexBuffer.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
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

TEST_F(MutexBufferTest, ConstructsWithCorrectCapacity)
{
    ASSERT_EQ(mutexBuffer_.capacity(), bufferSize);
    EXPECT_FALSE(mutexBuffer_.closed());
}

TEST(MutexBufferConstructionTest, ThrowsOnZeroSize)
{
    ASSERT_THROW(MutexBuffer<int>{0}, std::invalid_argument);
}

TEST_F(MutexBufferTest, TryPushOnEmptyReturnsTrue)
{
    ASSERT_TRUE(mutexBuffer_.try_push(0));
}

TEST_F(MutexBufferTest, FrontAfterPushReturnsValue)
{
    mutexBuffer_.try_push(0);

    int* val = mutexBuffer_.front();
    
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 0);
}

TEST_F(MutexBufferTest, FrontOnEmptyReturnsNullptr)
{
    ASSERT_EQ(mutexBuffer_.front(), nullptr);
}

TEST_F(MutexBufferTest, PopRemovesElement)
{
    mutexBuffer_.try_push(0);
    
    mutexBuffer_.pop();

    ASSERT_EQ(mutexBuffer_.front(), nullptr);
}

TEST_F(MutexBufferTest, CanFillToCapacity)
{
    for (std::size_t i = 0; i < bufferSize; i++)
    {
        ASSERT_TRUE(mutexBuffer_.try_push(static_cast<int>(i)));
    }
}

TEST_F(MutexBufferTest, TryPushOnFullReturnsFalse)
{
    for (std::size_t i = 0; i < bufferSize; i++)
    {
        mutexBuffer_.try_push(static_cast<int>(i));
    }

    ASSERT_FALSE(mutexBuffer_.try_push(bufferSize));
}

TEST_F(MutexBufferTest, CanPushAgainAfterPop)
{
    for (std::size_t i = 0; i < bufferSize; i++)
    {
        mutexBuffer_.push(static_cast<int>(i));
    }

    ASSERT_FALSE(mutexBuffer_.try_push(bufferSize));
    
    mutexBuffer_.pop();
    
    ASSERT_TRUE(mutexBuffer_.try_push(bufferSize + 1));
}

TEST_F(MutexBufferTest, PreservesOrderingAcrossPushPop)
{
    for (int i = 0; i < bufferSize; i++)
    {
        mutexBuffer_.push(i);
    }
    
    for (int i = 0; i < bufferSize; i++)
    {
        int* val = mutexBuffer_.front();

        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i);

        mutexBuffer_.pop();
    }
}

TEST_F(MutexBufferTest, WraparoundWorksCorrectly)
{
    for (int i = 0; i < bufferSize; i++) 
    {
        mutexBuffer_.push(i);
    }

    for (int i = 0; i < 2; i++)
    {
        EXPECT_EQ(*mutexBuffer_.front(), i);

        mutexBuffer_.pop();
    }

    for (int i = bufferSize; i < bufferSize + 2; i++)
    {
        ASSERT_TRUE(mutexBuffer_.try_push(i));
    }

    for (int i = 2; i < 4; i++)
    {
        ASSERT_EQ(*mutexBuffer_.front(), i);

        mutexBuffer_.pop();
    }
}

TEST(MutexBufferEmplaceTest, EmplacesWithMultipleArgs)
{
    MutexBuffer<std::pair<int, std::string>> buffer{4};

    buffer.emplace(0, "Ethan");

    auto* val = buffer.front();

    ASSERT_NE(val, nullptr);
    EXPECT_EQ(val->first, 0);
    EXPECT_EQ(val->second, "Ethan");
}

TEST(MutexBufferMoveOnlyTest, WorksWithUniquePtr)
{
    MutexBuffer<std::unique_ptr<int>> buffer{4};

    buffer.push(std::make_unique<int>(1));

    auto* val = buffer.front();

    ASSERT_NE(val, nullptr);
    ASSERT_NE(*val, nullptr);
    ASSERT_EQ(**val, 1);
}

struct Tracker
{
    static inline int aliveCount = 0;

    Tracker()
    {
        ++aliveCount;
    }

    Tracker(const Tracker&)
    {
        ++aliveCount;
    }

    Tracker(Tracker&&) noexcept
    {
        ++aliveCount;
    }

    Tracker& operator=(const Tracker&) = default;
    Tracker& operator=(Tracker&&) noexcept = default;

    ~Tracker()
    {
        --aliveCount;
    }
};

TEST(MutexBufferDestructorTest, DestroysRemainingElements)
{
    Tracker::aliveCount = 0;

    {
        MutexBuffer<Tracker> buffer{10};
        for (int i = 0; i < 5; ++i)
        {
            buffer.emplace();
        }

        ASSERT_EQ(Tracker::aliveCount, 5);
    }

    ASSERT_EQ(Tracker::aliveCount, 0);
}

TEST_F(MutexBufferTest, ClosedFalseInitially)
{
    ASSERT_FALSE(mutexBuffer_.closed());
}

TEST_F(MutexBufferTest, ClosedTrueAfterClose)
{
    mutexBuffer_.close();

    ASSERT_TRUE(mutexBuffer_.closed());
}

TEST_F(MutexBufferTest, TryPopReturnsNulloptOnEmpty)
{
    ASSERT_FALSE(mutexBuffer_.try_pop().has_value());
}

TEST_F(MutexBufferTest, TryPopReturnsValueWhenNonEmpty)
{
    mutexBuffer_.try_push(0);

    auto val = mutexBuffer_.try_pop();

    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 0);
}

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