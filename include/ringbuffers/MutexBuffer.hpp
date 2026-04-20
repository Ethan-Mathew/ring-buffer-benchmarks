#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace ringbuffers
{

template <typename T, typename Allocator = std::allocator<T>>
class MutexBuffer
{
public:
    explicit MutexBuffer(std::size_t bufferSize)
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
        while (count_ > 0)
        {
            std::destroy_at(std::addressof(buffer_[popCursor_]));

            popCursor_ = (popCursor_ + 1) % capacity_;
            count_--;
        }

        std::allocator_traits<Allocator>::deallocate(allocator_, buffer_, capacity_);
    }

    template <typename... Args>
    void push(Args&&... item)
    {
        std::unique_lock<std::mutex> lock{mutex_};

        cvFull_.wait(lock, [this]{return (count_ != capacity_) || closed_;});

        if (closed_)
        {
            return;
        }

        std::construct_at(std::addressof(buffer_[pushCursor_]), std::forward<Args>(item)...);

        pushCursor_ = (pushCursor_ + 1) % capacity_;
        count_++;

        lock.unlock();
    }

    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock{mutex_};

        std::optional<T> ret;

        if (count_ > 0)
        {
            ret = std::move(buffer_[popCursor_]);
            std::destroy_at(std::addressof(buffer_[popCursor_]));

            popCursor_ = (popCursor_ + 1) % capacity_;
            count_--;

            lock.unlock();
            cvFull_.notify_one();
        }
        
        return ret;
    }

    void close()
    {
        std::unique_lock<std::mutex> lock{mutex_};

        closed_ = true;

        lock.unlock();

        cvFull_.notify_all();
    }

    bool closed() const
    {
        std::scoped_lock<std::mutex> lock{mutex_};

        return closed_;
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
    bool closed_ = false;
    mutable std::mutex mutex_;
    std::condition_variable cvFull_;
};

} // namespace ringbuffers