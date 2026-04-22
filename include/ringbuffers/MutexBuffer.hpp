#pragma once

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ringbuffers
{

template <typename T, typename Allocator = std::allocator<T>>
class MutexBuffer
{
public:
    explicit MutexBuffer(std::size_t bufferSize)
    {
        if (bufferSize < 1)
        {
            throw std::invalid_argument("MutexBuffer capacity must be greater than or equal to 1");
        }

        capacity_ = bufferSize;

        buffer_ = std::allocator_traits<Allocator>::allocate(allocator_, bufferSize);
    }

    MutexBuffer(const MutexBuffer&) = delete;
    MutexBuffer& operator=(const MutexBuffer&) = delete;
    MutexBuffer(MutexBuffer&&) = delete;
    MutexBuffer& operator=(MutexBuffer&&) = delete;

    ~MutexBuffer()
    {
        while (count_ > 0)
        {
            std::destroy_at(std::addressof(buffer_[popCursor_]));

            popCursor_ = (popCursor_ + 1) % capacity_;
            count_--;
        }

        std::allocator_traits<Allocator>::deallocate(allocator_, buffer_, capacity_);
    }

    template <typename... Args>
    void emplace(Args&&... args)
    {
        std::unique_lock<std::mutex> lock{mutex_};

        cvFull_.wait(lock, [this]{return count_ != capacity_;});

        std::construct_at(std::addressof(buffer_[pushCursor_]), std::forward<Args>(args)...);

        pushCursor_ = (pushCursor_ + 1) % capacity_;
        count_++;
    }

    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        std::scoped_lock<std::mutex> lock{mutex_};

        if (count_ == capacity_)
        {
            return false;
        }

        std::construct_at(std::addressof(buffer_[pushCursor_]), std::forward<Args>(args)...);

        pushCursor_ = (pushCursor_ + 1) % capacity_;
        count_++;

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

    // Same contract and API as SPSC - returns pointer to first consumable element
    // NOT safe for multi-consumer use due to pointer access to potentially shared data
    T* front()
    {
        std::scoped_lock<std::mutex> lock{mutex_};

        if (count_ == 0)
        {
            return nullptr;
        }

        return std::addressof(buffer_[popCursor_]);
    }

    // Safe for multi-consumer use
    // Returns std::nullopt on empty pops
    std::optional<T> try_pop()
    {
        std::unique_lock<std::mutex> lock{mutex_};

        std::optional<T> value;

        if (count_ > 0)
        {
            value = std::move(buffer_[popCursor_]);
            std::destroy_at(std::addressof(buffer_[popCursor_]));

            popCursor_ = (popCursor_ + 1) % capacity_;
            count_--;

            lock.unlock();
            cvFull_.notify_one();
        }
        
        return value;
    }

    // Same contract as SPSC for benchmarking
    // Assumes front() has been called to prevent pop on empty
    // NOT safe for multi-consumer use
    void pop()
    {
        std::unique_lock<std::mutex> lock{mutex_};

        assert(count_ > 0);

        std::destroy_at(std::addressof(buffer_[popCursor_]));
    
        popCursor_ = (popCursor_ + 1) % capacity_;
        count_--;

        lock.unlock();
        cvFull_.notify_one();
    }

    std::size_t capacity() const
    {
        return capacity_;
    }

    void close()
    {
        std::scoped_lock<std::mutex> lock{mutex_};

        closed_ = true;
    }

    bool closed() const
    {
        std::scoped_lock<std::mutex> lock{mutex_};

        return closed_;
    }

private:
    T* buffer_;
    Allocator allocator_;
    std::size_t capacity_;
    std::size_t count_ = 0;
    std::size_t pushCursor_ = 0;
    std::size_t popCursor_ = 0;
    bool closed_ = false;
    mutable std::mutex mutex_;
    std::condition_variable cvFull_;
};

} // namespace ringbuffers