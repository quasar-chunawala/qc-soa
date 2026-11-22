#ifndef VECTOR_FLAT_H
#define VECTOR_FLAT_H

#include <algorithm>
#include <cstddef>
#include <memory>
#include <meta>
#include <print>
#include <ranges>

#include <contracts/contract_assert.h>
#include <soa/split_buffer.h>

// Godbolt: https://compiler-explorer.com/z/Kaq1aT9TE

namespace soa {
namespace flat_internal {

template <typename T, typename Alloc = std::allocator<T>>
class vector_flat {
    using SplitBuffer = soa::detail::split_buffer<T, Alloc>;
    using value_type = T;
    using allocator_type = Alloc;
    using reference = T&;
    using const_reference = const T&;

public:
    constexpr vector_flat() = default;
    explicit vector_flat(size_t n) {
        auto guard = exception_guard(destroy_vector(*this));
        std::byte* raw_mem = buffer_.allocate_memory(n);
        buffer_.set_pointers(raw_mem, n);
        buffer_.construct_at_end_with_size(n, T());
        guard.complete();
    }

    explicit vector_flat(size_t n, const_reference value) {
        auto guard = exception_guard(destroy_vector(*this));
        std::byte* raw_mem = buffer_.allocate_memory(n);
        buffer_.set_pointers(raw_mem, n);
        buffer_.construct_at_end_with_size(n, value);
        guard.complete();
    }
    void clear() { buffer_.clear(); }
    ~vector_flat() { destroy_vector(*this); }

private:
    SplitBuffer buffer_{};

    struct destroy_vector {
        explicit destroy_vector(vector_flat& vec)
            : vec_{vec} {}

        constexpr void operator()() {
            vec_.clear();
            vec_.buffer_.deallocate_memory();
            vec_.buffer_.reset();
        }
        vector_flat vec_;
    };

    template <typename InputIterator, typename Sentinel>
    void init_with_size(InputIterator first, Sentinel last, size_t n) {
        auto guard = exception_guard(destroy_vector(*this));
        if (n > 0) {
            buffer_.allocate(n);
            buffer_.construct_at_end_with_size(first, last);
        }
        guard.complete();
    }

    template <typename InputIterator, typename Sentinel>
    void init_with_sentinel(InputIterator first, Sentinel last) {
        auto guard = exception_guard(destroy_vector(*this));
        for (; first != last; ++first) {
            emplace_back(*first);
        }
        guard.complete();
    }
};
}; // namespace flat_internal
}; // namespace soa

#endif