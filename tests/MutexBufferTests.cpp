#include <gtest/gtest.h>

#include <ringbuffers/MutexBuffer.hpp>

#include <cstddef>

using namespace ringbuffers;

static constexpr std::size_t bufferSize = 1000;

class MutexBufferTest : public testing::Test
{
protected:
    MutexBufferTest()
        : mrb_{bufferSize}
    {
    }

    /*
    void expect_empty_book()
    {
        EXPECT_EQ(ob_.get_num_orders(), 0);
        EXPECT_EQ(ob_.get_num_levels_bids(), 0);
        EXPECT_EQ(ob_.get_num_levels_asks(), 0);
        EXPECT_EQ(ob_.get_memory_pool_curr_alloc(), 0);
    }
    */

    MutexBuffer<int> mrb_;
};

TEST_F(MutexBufferTest, MutexBufferConstructs)
{
    ASSERT_EQ(mrb_.capacity(), bufferSize);
    EXPECT_FALSE(mrb_.closed());
    EXPECT_TRUE(mrb_.empty());
}