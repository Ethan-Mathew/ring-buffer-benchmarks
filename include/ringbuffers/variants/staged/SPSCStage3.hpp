/*
    STAGED OPTIMIZATIONS:

    + MEMORY ORDERING
    + CACHE LINE ALIGNMENT
    + CACHED PUSH/POP NON-ATOMIC CURSORS
*/

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ringbuffers::variants::staged
{

template <typename T, typename Allocator = std::allocator<T>>
class SPSCBufferStage3
{
// If there exists native support for std::hardware_destructive_interference_size
#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t cacheLineSize = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t cacheLineSize = 64;
#endif

public:
    explicit SPSCBufferStage3(std::size_t bufferSize)
    {
        if (bufferSize < 1)
        {
            throw std::invalid_argument("SPSCBufferStage3 capacity must be greater than or equal to 1");
        }
        
        // Slack element for cursor arithmetic
        capacity_ = bufferSize + 1;

        buffer_ = std::allocator_traits<Allocator>::allocate(allocator_, capacity_);
    }

    SPSCBufferStage3(const SPSCBufferStage3&) = delete;
    SPSCBufferStage3& operator=(const SPSCBufferStage3&) = delete;
    SPSCBufferStage3(SPSCBufferStage3&&) = delete;
    SPSCBufferStage3& operator=(SPSCBufferStage3&&) = delete;

    ~SPSCBufferStage3()
    {
        while (front())
        {
            pop();
        }

        std::allocator_traits<Allocator>::deallocate(allocator_, buffer_, capacity_);
    }

    template <typename... Args>
    void emplace(Args&&... args)
    {
        const std::size_t pushCursor = pushCursor_.load(std::memory_order_relaxed);
        const std::size_t pushCursorNext = (pushCursor + 1) % capacity_;

        // Spin until at least one space ahead of pushCursor
        while (pushCursorNext == popCursorCache_)
        {
            popCursorCache_ = popCursor_.load(std::memory_order_acquire);
        }

        std::construct_at(std::addressof(buffer_[pushCursor]), 
                          std::forward<Args>(args)...);

        pushCursor_.store(pushCursorNext, std::memory_order_release);
    }

    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        const std::size_t pushCursor = pushCursor_.load(std::memory_order_relaxed);
        const std::size_t pushCursorNext = (pushCursor + 1) % capacity_;

        // Queue full
        if (pushCursorNext == popCursorCache_)
        {
            // Refresh popCursorCache_ with latest value of popCursor_
            popCursorCache_ = popCursor_.load(std::memory_order_acquire);

            if (pushCursorNext == popCursorCache_)
            {
                return false;
            }
        }

        std::construct_at(std::addressof(buffer_[pushCursor]), 
                          std::forward<Args>(args)...);

        pushCursor_.store(pushCursorNext, std::memory_order_release);

        return true;
    }

    template <typename U>
    requires std::is_constructible_v<T, U&&>
    void push(U&& value)
    {
        emplace(std::forward<U>(value));
    }

    template <typename U>
    requires std::is_constructible_v<T, U&&>
    bool try_push(U&& value)
    {
        return try_emplace(std::forward<U>(value));
    }

    T* front()
    {
        const std::size_t popCursor = popCursor_.load(std::memory_order_relaxed);
        
        // Empty
        if (popCursor == pushCursorCache_)
        {
            pushCursorCache_ = pushCursor_.load(std::memory_order_acquire);

            if (popCursor == pushCursorCache_)
            {
                return nullptr;
            }
        }
        
        return std::addressof(buffer_[popCursor]);
    }

    // Assumes front() has been called to avoid pop on empty();
    void pop()
    {
        const std::size_t popCursor = popCursor_.load(std::memory_order_relaxed);

        assert(popCursor != pushCursor_.load(std::memory_order_acquire));

        std::destroy_at(std::addressof(buffer_[popCursor]));

        popCursor_.store((popCursor + 1) % capacity_, std::memory_order_release);
    }

    std::size_t capacity() const
    {
        return capacity_ - 1;
    }

    void close()
    {
        closed_.store(true, std::memory_order_release);
    }

    bool closed() const
    {
        return closed_.load(std::memory_order_acquire);
    }

private:
    T* buffer_;

    alignas(cacheLineSize) std::atomic<std::size_t> pushCursor_ = 0;
    std::size_t popCursorCache_ = 0;

    alignas(cacheLineSize) std::atomic<std::size_t> popCursor_ = 0;
    std::size_t pushCursorCache_ = 0;

    std::size_t capacity_;
    std::atomic<bool> closed_ = false;

    [[no_unique_address]] Allocator allocator_;
};

} // namespace ringbuffers::variants::staged