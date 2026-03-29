#pragma once

/*
 *  hpc-workspace-v2
 *
 *  PathWorkStealingQueue
 *
 *  - helper class for work stealing queue for ws_stat
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2026
 *
 *  hpc-workspace-v2 is based on workspace by Holger Berger, Thomas Beisel and Martin Hecht
 *
 *  hpc-workspace-v2 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  hpc-workspace-v2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with workspace-ng  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <thread>
#include <utility>

/**
 * @class PathWorkStealingQueue
 * @brief Lock-free work-stealing queue for any movable type (default: std::filesystem::path)
 *
 * Uses index-based synchronization with atomics only for counters.
 * Items are stored in a separate non-atomic array with occupied flags.
 *
 * - push() and pop() are owner-only operations
 * - steal() can be called by any worker thread
 * - Fallback blocking wait prevents queue exhaustion
 */
template <typename T = std::filesystem::path> class PathWorkStealingQueue {
    struct Block {
        T item;
        std::atomic<bool> occupied{false};
    };

    static constexpr size_t DEFAULT_CAPACITY = 1024;

    static constexpr size_t getAlignSize() {
#ifdef __cpp_lib_hardware_interference_size
        return std::hardware_destructive_interference_size;
#else
        return 64; // Typical cache line size
#endif
    }

    alignas(getAlignSize()) std::atomic<std::int64_t> bottom_;
    alignas(getAlignSize()) std::atomic<std::int64_t> top_;
    Block* array_;
    size_t capacity_;

  public:
    explicit PathWorkStealingQueue(size_t capacity = DEFAULT_CAPACITY)
        : bottom_(0), top_(0), array_(new Block[capacity]), capacity_(capacity) {}

    ~PathWorkStealingQueue() { delete[] array_; }

    // Prevent copying and moving
    PathWorkStealingQueue(const PathWorkStealingQueue&) = delete;
    PathWorkStealingQueue& operator=(const PathWorkStealingQueue&) = delete;
    PathWorkStealingQueue(PathWorkStealingQueue&&) = delete;
    PathWorkStealingQueue& operator=(PathWorkStealingQueue&&) = delete;

    /**
     * @brief Push an item into the queue (owner only)
     *
     * If the queue is nearly full, blocks briefly to allow thieves to catch up.
     *
     * @param item The item to push (moved into queue)
     */
    void push(T&& item) {
        std::int64_t b = bottom_.load(std::memory_order_relaxed);
        std::int64_t t = top_.load(std::memory_order_relaxed);

        // Fallback: if queue is nearly full, yield to let thieves catch up
        size_t limit = capacity_ - (capacity_ / 10); // 90% threshold
        while (static_cast<size_t>(b - t) >= limit) {
            std::this_thread::yield();
            b = bottom_.load(std::memory_order_relaxed);
            t = top_.load(std::memory_order_relaxed);
        }

        array_[static_cast<size_t>(b)].item = std::move(item);

        // Ensure item is visible before marking as occupied
        std::atomic_thread_fence(std::memory_order_release);

        bottom_.store(b + 1, std::memory_order_relaxed);

        // Mark as logically occupied (for debugging, not used in algorithm)
        // array_[b].occupied.store(true, std::memory_order_relaxed);
    }

    /**
     * @brief Pop an item from the queue (owner only)
     *
     * @return std::optional<T> The popped item, or nullopt if empty
     */
    std::optional<T> pop() {
        std::int64_t b = bottom_.load(std::memory_order_relaxed) - 1;

        bottom_.store(b, std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        std::int64_t t = top_.load(std::memory_order_relaxed);

        std::optional<T> item;
        if (t <= b) {
            item = std::move(array_[static_cast<size_t>(b)].item);

            if (t == b) {
                // This might be the last item - check for race with thief
                if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    // Thief stole the item
                    item.reset();
                }
                bottom_.store(b + 1, std::memory_order_relaxed);
            }
        } else {
            // Queue was empty
            bottom_.store(b + 1, std::memory_order_relaxed);
        }

        return item;
    }

    /**
     * @brief Steal an item from the queue (any thread)
     *
     * @return std::optional<T> The stolen item, or nullopt if empty
     */
    std::optional<T> steal() {
        std::int64_t t = top_.load(std::memory_order_acquire);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        std::int64_t b = bottom_.load(std::memory_order_acquire);

        if (t < b) {
            // Try to claim this item
            if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                // Owner popped it first or another thief stole it
                return std::nullopt;
            }
            // Successfully claimed - move and return
            return std::move(array_[static_cast<size_t>(t)].item);
        }

        return std::nullopt;
    }

    /**
     * @brief Check if queue appears empty (approximate)
     *
     * May return false positive due to concurrent modifications.
     */
    bool empty() const noexcept {
        std::int64_t b = bottom_.load(std::memory_order_relaxed);
        std::int64_t t = top_.load(std::memory_order_relaxed);
        return b <= t;
    }

    /**
     * @brief Approximate size of queue (approximate)
     *
     * May return stale value due to concurrent modifications.
     */
    size_t size() const noexcept {
        std::int64_t b = bottom_.load(std::memory_order_relaxed);
        std::int64_t t = top_.load(std::memory_order_relaxed);
        return b >= t ? static_cast<size_t>(b - t) : 0;
    }

    /**
     * @brief Get capacity of the queue
     */
    size_t capacity() const noexcept { return capacity_; }
};
