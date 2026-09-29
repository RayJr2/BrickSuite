#include <mcut/internal/tpool.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

int main()
{
    // Repeated empty-to-nonempty transitions exercise the producer notification
    // racing the consumer's predicate-check/wait boundary in MCUT's real queue.
    constexpr int iterations = 100000;
    std::atomic<bool> done{false};
    std::atomic<int> acknowledged{0};
    thread_safe_queue<int> queue;
    queue.set_done_ptr(&done);
    std::thread consumer([&] {
        for (int expected = 1; expected <= iterations; ++expected) {
            int value = 0;
            queue.wait_and_pop(value);
            if (value != expected) {
                std::fputs("MCUT queue changed task order\n", stderr);
                std::_Exit(1);
            }
            acknowledged.store(value, std::memory_order_release);
        }
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (int value = 1; value <= iterations; ++value) {
        queue.push(value);
        while (acknowledged.load(std::memory_order_acquire) != value) {
            if (std::chrono::steady_clock::now() >= deadline) {
                std::fputs("MCUT queue did not wake for a submitted task\n", stderr);
                std::_Exit(1); // Do not hang while joining a sleeping consumer.
            }
            std::this_thread::yield();
        }
    }
    consumer.join();
    return 0;
}
