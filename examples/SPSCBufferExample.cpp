/*
    EXAMPLE SAFE USAGE OF SPSCBuffer
*/

#include <ringbuffers/SPSCBuffer.hpp>

#include <atomic>
#include <cstddef>
#include <iostream>
#include <thread>

using namespace ringbuffers;

static constexpr std::size_t bufferSize = 1024;
static constexpr std::size_t itemCount = 1'000'000;

int main()
{
    SPSCBuffer<int> buffer{bufferSize};

    std::thread producer{[&]
    {
        for (std::size_t i = 0; i < itemCount; ++i)
        {
            buffer.push(static_cast<int>(i));
        }

        // Notify that production has halted
        buffer.close();
    }};

    std::thread consumer{[&]
    {
        while (true)
        {
            auto* val = buffer.front();

            if (val)
            {
                std::cout << *val << '\n';

                buffer.pop();
            }
            else if (buffer.closed()) // Check if production is complete
            {
                break;
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }};

    producer.join();
    consumer.join();
}