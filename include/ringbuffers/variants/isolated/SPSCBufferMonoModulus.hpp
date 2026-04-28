/*
    ISOLATED OPTIMIZATIONS

    + MONOTONIC INDICES REGULARIZED BY MODULUS OPERATOR
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
class SPSCBufferMonoModulus
{
public:
    explicit SPSCBufferMonoModulus(std::size_t bufferSize)
    {
        if (bufferSize < 1)
        {
            throw std::invalid_argument("SPSCBufferMonoModulus capacity must be greater than or equal to 1");
        }
        
        // Slack element for cursor arithmetic
        capacity_ = bufferSize + 1;

        buffer_ = std::allocator_traits<Allocator>::allocate(allocator_, capacity_);
    }

    SPSCBufferMonoModulus(const SPSCBufferMonoModulus&) = delete;
    SPSCBufferMonoModulus& operator=(const SPSCBufferMonoModulus&) = delete;
    SPSCBufferMonoModulus(SPSCBufferMonoModulus&&) = delete;
    SPSCBufferMonoModulus& operator=(SPSCBufferMonoModulus&&) = delete;

    ~SPSCBufferMonoModulus()
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
        const std::size_t pushCursor = pushCursor_.load(std::memory_order_seq_cst);
        const std::size_t pushCursorNext = pushCursor + 1;

        // Queue full
        while (pushCursor - popCursor_.load(std::memory_order_seq_cst) == capacity_);

        std::construct_at(std::addressof(buffer_[pushCursor % capacity_]), 
                          std::forward<Args>(args)...);

        pushCursor_.store(pushCursorNext, std::memory_order_seq_cst);
    }

    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        const std::size_t pushCursor = pushCursor_.load(std::memory_order_seq_cst);
        const std::size_t pushCursorNext = pushCursor + 1;

        // Queue full
        if (pushCursor - popCursor_.load(std::memory_order_seq_cst) == capacity_)
        {
            return false;
        }

        std::construct_at(std::addressof(buffer_[pushCursor % capacity_]), 
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
        
        return std::addressof(buffer_[popCursor % capacity_]);
    }

    // Assumes front() has been called to avoid pop on empty();
    void pop()
    {
        const std::size_t popCursor = popCursor_.load(std::memory_order_seq_cst);

        assert(popCursor != pushCursor_.load(std::memory_order_seq_cst));

        std::destroy_at(std::addressof(buffer_[popCursor % capacity_]));

        popCursor_.store(popCursor + 1, std::memory_order_seq_cst);
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
    T* buffer_;

    std::atomic<std::size_t> pushCursor_ = 0;
    std::atomic<std::size_t> popCursor_ = 0;
    
    std::size_t capacity_;
    std::atomic<bool> closed_ = false;

    [[no_unique_address]] Allocator allocator_;
};

} // namespace ringbuffers::variants::isolated