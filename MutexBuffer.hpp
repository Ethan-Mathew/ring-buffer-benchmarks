#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>

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

    ~MutexBuffer()
    {
        std::allocator_traits<Allocator>::deallocate(allocator_, buffer_, capacity_);
    }

    void push(T item)
    {
        std::unique_lock<std::mutex> lock{mutex_};

        if (count_ == capacity_)
        {
            cvFull_.wait(lock, [this]{return count_ != capacity_;});
        }
        
        std::cout << "Pushing: " << item << std::endl;

        std::construct_at(std::addressof(buffer_[pushCursor_]), item);
        pushCursor_ = (pushCursor_ + 1) % capacity_;

        count_++;

        lock.unlock();

        cvEmpty_.notify_one();
    }

    void pop()
    {
        std::unique_lock<std::mutex> lock{mutex_};

        if (count_ == 0)
        {
            cvEmpty_.wait(lock, [this]{return count_ > 0;});
        }

        std::cout << "Popping: " << buffer_[popCursor_] << std::endl;

        std::destroy_at(std::addressof(buffer_[popCursor_]));

        popCursor_ = (popCursor_ + 1) % capacity_;

        count_--;

        lock.unlock();

        cvFull_.notify_one();
    }

    bool empty() const
    {
        std::scoped_lock<std::mutex> lock{mutex_};

        return pushCursor_ == popCursor_;
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