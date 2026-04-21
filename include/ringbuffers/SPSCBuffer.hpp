#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace ringbuffers
{

template <typename T, typename Allocator = std::allocator<T>>
class SPSCBuffer
{
// If there exists native support for std::hardware_destructive_interference_size
#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t cacheLineSize = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t cacheLineSize = 64;
#endif

    // Calculate # of objects per cache line (rounded up)
    static constexpr std::size_t padding = (cacheLineSize + sizeof(T) - 1) / sizeof(T);

public:
    explicit SPSCBuffer(std::size_t bufferSize)
    {
        if (bufferSize < 1)
        {
            throw std::invalid_argument("SPSCBuffer capacity must be greater than or equal to 1");
        }
        
        // Slack element for cursor arithmetic
        capacity_ = bufferSize + 1;

        //
        // Come back to later for optimization!
        //
        /*
        // Enforce capacity == power of two
        capacity_ = std::bit_ceil(bufferSize);
        */

        // Add two padding cache lines to prevent contention over first and last object slots
        buffer_ = std::allocator_traits<Allocator>::allocate(allocator_, capacity_);

        //assert(difference between atomic members is equal to a cache line)
    }

    SPSCBuffer(const SPSCBuffer&) = delete;
    SPSCBuffer& operator=(const SPSCBuffer&) = delete;
    SPSCBuffer(SPSCBuffer&&) = delete;
    SPSCBuffer& operator=(SPSCBuffer&&) = delete;

    ~SPSCBuffer()
    {
        // to do: destroy objects before deallocation
        std::allocator_traits<Allocator>::deallocate(allocator_, buffer_, capacity_);
    }

    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        const std::size_t pushCursor = pushCursor_.load(std::memory_order_seq_cst);
        std::size_t pushCursorNext = (pushCursor + 1) % capacity_;

        // Queue full
        if (pushCursorNext == popCursor_.load(std::memory_order_seq_cst))
        {
            return false;
        }

        std::construct_at(std::addressof(buffer_[pushCursor]), 
                          std::forward<Args>(args)...);

        pushCursor_.store(pushCursorNext, std::memory_order_seq_cst);

        return true;
    }

    bool try_push(const T& value)
    {
        return try_emplace(value);
    }

    bool try_push(T&& value)
    {
        return try_emplace(std::move(value));
    }

    T* front()
    {
        const std::size_t popCursor = popCursor_.load(std::memory_order_seq_cst);
        
        // Empty
        if (popCursor == pushCursor_.load(std::memory_order_seq_cst))
        {
            return nullptr;
        }
        
        return std::addressof(buffer_[popCursor]);
    }

    // Assumes front() has been called to avoid pop on empty();
    void pop()
    {
        const std::size_t popCursor = popCursor_.load(std::memory_order_seq_cst);

        assert(popCursor != pushCursor_.load(std::memory_order_seq_cst));

        std::destroy_at(std::addressof(buffer_[popCursor]));

        popCursor_.store((popCursor + 1) % capacity_, std::memory_order_seq_cst);
    }

    std::size_t capacity() const
    {
        return capacity_ - 1;
    }

private:
    alignas(cacheLineSize) T* buffer_;

    std::atomic<std::size_t> pushCursor_ = 0;
    // std::size_t popCursorCache_;

    std::atomic<std::size_t> popCursor_ = 0;
    // std::size_t pushCursorCache_;

    std::size_t capacity_;
    [[no_unique_address]] Allocator allocator_;

    // Sanity checks for class alignment
    //static_assert(alignof(SPSCBuffer) == cacheLineSize, 
    //            "SPSCBuffer is not aligned along native cache lines");
    //static_assert(sizeof(SPSCBuffer) >= 4 * cacheLineSize, 
    //            "SPSCBuffer's size is less than four aligned atomics");

};

} // namespace ringbuffers