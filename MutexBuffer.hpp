#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace rbb
{

template <typename T, typename Allocator = std::allocator<T>>
class MutexBuffer
{
public:
    MutexBuffer(std::size_t bufferSize)
        : capacity_{bufferSize}
    {
        buffer_ = std::allocator_traits<Allocator>::allocate(allocator_, bufferSize);
    }

    MutexBuffer(const MutexBuffer&) = delete;
    MutexBuffer& operator=(const MutexBuffer&) = delete;
    MutexBuffer(MutexBuffer&&) = delete;
    MutexBuffer& operator=(MutexBuffer&&) = delete;

    ~MutexBuffer()
    {
        std::allocator_traits<Allocator>::deallocate(allocator_, buffer_, capacity_);
    }

    void push(T item)
    {
        std::unique_lock<std::mutex> lock{mutex_};

        cvFull_.wait(lock, [this]{return count_ != capacity_;});

        std::construct_at(std::addressof(buffer_[pushCursor_]), item);

        pushCursor_ = (pushCursor_ + 1) % capacity_;
        count_++;

        lock.unlock();

        cvEmpty_.notify_one();
    }

    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock{mutex_};

        cvEmpty_.wait(lock, [this]{return count_ > 0;});

        std::optional<T> ret = std::move(buffer_[popCursor_]);
        std::destroy_at(std::addressof(buffer_[popCursor_]));

        popCursor_ = (popCursor_ + 1) % capacity_;
        count_--;

        lock.unlock();
        cvFull_.notify_one();
        
        return ret;
    }

    bool empty() const
    {
        std::scoped_lock<std::mutex> lock{mutex_};

        return count_ == 0;
    }

    std::size_t capacity() const
    {
        return capacity_;
    }

private:
    T* buffer_;
    Allocator allocator_;
    std::size_t capacity_;
    std::size_t count_ = 0;
    std::size_t pushCursor_ = 0;
    std::size_t popCursor_ = 0;
    mutable std::mutex mutex_;
    std::condition_variable cvFull_;
    std::condition_variable cvEmpty_;
};

} // namespace rbb