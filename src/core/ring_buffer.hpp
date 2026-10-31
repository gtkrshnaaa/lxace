#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <cstddef>
#include <algorithm>

namespace lxace::core {

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : capacity_(capacity > 0 ? capacity : 1) {}

    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.size() < capacity_) {
            buffer_.push_back(item);
        } else {
            buffer_[head_] = item;
            head_ = (head_ + 1) % capacity_;
            is_full_ = true;
        }
    }

    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.size() < capacity_) {
            buffer_.push_back(std::move(item));
        } else {
            buffer_[head_] = std::move(item);
            head_ = (head_ + 1) % capacity_;
            is_full_ = true;
        }
    }

    [[nodiscard]] std::vector<T> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> result;
        result.reserve(buffer_.size());

        if (!is_full_) {
            result = buffer_;
        } else {
            for (size_t i = 0; i < capacity_; ++i) {
                result.push_back(buffer_[(head_ + i) % capacity_]);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<T> tail(size_t n) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> all;
        if (!is_full_) {
            all = buffer_;
        } else {
            all.reserve(capacity_);
            for (size_t i = 0; i < capacity_; ++i) {
                all.push_back(buffer_[(head_ + i) % capacity_]);
            }
        }

        if (all.size() <= n) {
            return all;
        }

        size_t start = all.size() - n;
        return std::vector<T>(all.begin() + static_cast<std::ptrdiff_t>(start), all.end());
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

    [[nodiscard]] size_t capacity() const {
        return capacity_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.clear();
        head_ = 0;
        is_full_ = false;
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;
    size_t head_{0};
    bool is_full_{false};
    mutable std::mutex mutex_;
};

} // namespace lxace::core
