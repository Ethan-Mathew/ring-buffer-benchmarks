#include <gtest/gtest.h>

#include <ringbuffers/SPSCBuffer.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>

using namespace ringbuffers;
using namespace std::chrono_literals;

static constexpr std::size_t bufferSize = 1024;

class SPSCBufferTest : public testing::Test
{
protected:
    SPSCBufferTest()
        : spscBuffer_{bufferSize}
    {
    }

    SPSCBuffer<int> spscBuffer_;
};

TEST_F(SPSCBufferTest, ConstructsWithCorrectCapacity)
{
    ASSERT_EQ(spscBuffer_.capacity(), bufferSize);
    EXPECT_FALSE(spscBuffer_.closed());
}

TEST(SPSCBufferConstructionTest, ThrowsOnZeroSize)
{
    ASSERT_THROW(SPSCBuffer<int>{0}, std::invalid_argument);
}

TEST_F(SPSCBufferTest, TryPushOnEmptyReturnsTrue)
{
    ASSERT_TRUE(spscBuffer_.try_push(0));
}

TEST_F(SPSCBufferTest, FrontAfterPushReturnsValue)
{
    ASSERT_TRUE(spscBuffer_.try_push(0));

    auto* val = spscBuffer_.front();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 0);
}

TEST_F(SPSCBufferTest, FrontOnEmptyReturnsNullptr)
{
    ASSERT_EQ(spscBuffer_.front(), nullptr);
}

TEST_F(SPSCBufferTest, PopRemovesElement)
{
    ASSERT_TRUE(spscBuffer_.try_push(0));

    auto* val = spscBuffer_.front();
    ASSERT_NE(val, nullptr);

    spscBuffer_.pop();
    ASSERT_EQ(spscBuffer_.front(), nullptr);
}

TEST_F(SPSCBufferTest, CanFillToCapacity)
{
    for (int i = 0; i < bufferSize; ++i)
    {
        ASSERT_TRUE(spscBuffer_.try_push(i));
    }
}

TEST_F(SPSCBufferTest, TryPushOnFullReturnsFalse)
{
    for (int i = 0; i < bufferSize; ++i)
    {
        ASSERT_TRUE(spscBuffer_.try_push(i));
    }

    ASSERT_FALSE(spscBuffer_.try_push(static_cast<int>(bufferSize)));
}

TEST_F(SPSCBufferTest, CanPushAgainAfterPop)
{
    for (int i = 0; i < bufferSize; ++i)
    {
        spscBuffer_.push(i);
    }

    ASSERT_FALSE(spscBuffer_.try_push(static_cast<int>(bufferSize)));

    auto* val = spscBuffer_.front();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 0);

    spscBuffer_.pop();

    ASSERT_TRUE(spscBuffer_.try_push(static_cast<int>(bufferSize + 1)));
}

TEST_F(SPSCBufferTest, PreservesOrderingAcrossPushPop)
{
    for (int i = 0; i < bufferSize; ++i)
    {
        spscBuffer_.push(i);
    }

    for (int i = 0; i < bufferSize; ++i)
    {
        auto* val = spscBuffer_.front();
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i);

        spscBuffer_.pop();
    }
}

TEST_F(SPSCBufferTest, WraparoundWorksCorrectly)
{
    for (int i = 0; i < bufferSize; ++i)
    {
        spscBuffer_.push(i);
    }

    for (int i = 0; i < 2; ++i)
    {
        auto* val = spscBuffer_.front();
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i);

        spscBuffer_.pop();
    }

    for (int i = bufferSize; i < bufferSize + 2; ++i)
    {
        ASSERT_TRUE(spscBuffer_.try_push(i));
    }

    for (int i = 2; i < 4; ++i)
    {
        auto* val = spscBuffer_.front();
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i);

        spscBuffer_.pop();
    }
}

TEST(SPSCBufferEmplaceTest, EmplacesWithMultipleArgs)
{
    SPSCBuffer<std::pair<int, std::string>> buffer{4};

    buffer.emplace(0, "Ethan");

    auto* val = buffer.front();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(val->first, 0);
    EXPECT_EQ(val->second, "Ethan");
}

TEST(SPSCBufferMoveOnlyTest, WorksWithUniquePtr)
{
    SPSCBuffer<std::unique_ptr<int>> buffer{4};

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

TEST(SPSCBufferDestructorTest, DestroysRemainingElements)
{
    Tracker::aliveCount = 0;

    {
        SPSCBuffer<Tracker> buffer{10};

        for (int i = 0; i < 5; ++i)
        {
            buffer.emplace();
        }

        ASSERT_EQ(Tracker::aliveCount, 5);
    }

    ASSERT_EQ(Tracker::aliveCount, 0);
}

TEST_F(SPSCBufferTest, ClosedFalseInitially)
{
    ASSERT_FALSE(spscBuffer_.closed());
}

TEST_F(SPSCBufferTest, ClosedTrueAfterClose)
{
    spscBuffer_.close();
    ASSERT_TRUE(spscBuffer_.closed());
}

TEST(SPSCBufferPushTest, PushBlocksUntilSpaceIsAvailable)
{
    SPSCBuffer<int> buffer{1};

    buffer.push(0);

    std::atomic<bool> producerStarted = false;
    std::atomic<bool> producerFinished = false;

    std::thread producer{[&]
    {
        producerStarted.store(true, std::memory_order_relaxed);
        buffer.push(1);
        producerFinished.store(true, std::memory_order_relaxed);
    }};

    while (!producerStarted.load(std::memory_order_relaxed))
    {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(producerFinished.load(std::memory_order_relaxed));

    auto* val = buffer.front();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 0);

    buffer.pop();
    producer.join();

    EXPECT_TRUE(producerFinished.load(std::memory_order_relaxed));

    val = buffer.front();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 1);
}