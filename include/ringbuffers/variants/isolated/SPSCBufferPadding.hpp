/*
    ISOLATED OPTIMIZATIONS

    + BUFFER PADDING
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

namespace ringbuffers::variants::isolated
{

template <typename T, typename Allocator = std::allocator<T>>
class SPSCBufferPadding
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
    explicit SPSCBufferPadding(std::size_t bufferSize)
    {
        if (bufferSize < 1)
        {
            throw std::invalid_argument("SPSCBufferPadding capacity must be greater than or equal to 1");
        }
        
        // Slack element for cursor arithmetic
        capacity_ = bufferSize + 1;

        // Add two padding cache lines to prevent contention over first and last object slots
        rawBuffer_ = std::allocator_traits<Allocator>::allocate(allocator_, capacity_ + 2 * padding);
        buffer_ = rawBuffer_ + padding;
    }

    SPSCBufferPadding(const SPSCBufferPadding&) = delete;
    SPSCBufferPadding& operator=(const SPSCBufferPadding&) = delete;
    SPSCBufferPadding(SPSCBufferPadding&&) = delete;
    SPSCBufferPadding& operator=(SPSCBufferPadding&&) = delete;

    ~SPSCBufferPadding()
    {
        while (front())
        {
            pop();
        }

        std::allocator_traits<Allocator>::deallocate(allocator_, rawBuffer_, capacity_ + 2 * padding);
    }

    template <typename... Args>
    void emplace(Args&&... args)
    {
        const std::size_t pushCursor = pushCursor_.load(std::memory_order_seq_cst);
        const std::size_t pushCursorNext = (pushCursor + 1) % capacity_;

        // Spin until at least one space ahead of pushCursor
        while (pushCursorNext == popCursor_.load(std::memory_order_seq_cst));

        std::construct_at(std::addressof(buffer_[pushCursor]), 
                          std::forward<Args>(args)...);

        pushCursor_.store(pushCursorNext, std::memory_order_seq_cst);
    }

    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        const std::size_t pushCursor = pushCursor_.load(std::memory_order_seq_cst);
        const std::size_t pushCursorNext = (pushCursor + 1) % capacity_;

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

    void close()
    {
        closed_.store(true, std::memory_order_seq_cst);
    }

    bool closed() const
    {
        return closed_.load(std::memory_order_seq_cst);
    }

private:
    T* rawBuffer_ = nullptr;
    T* buffer_ = nullptr;

    std::atomic<std::size_t> pushCursor_ = 0;
    std::atomic<std::size_t> popCursor_ = 0;
    
    std::size_t capacity_;
    std::atomic<bool> closed_ = false;

    [[no_unique_address]] Allocator allocator_;
};

} // namespace ringbuffers::variants::isolated