#include <utility>

/**
 * @brief `exception_guard` is a helper class for writing code with  strong exception guarantee.
 *
 * When writing code that can throw an exception, once can store rollback instructions in an exception guard, so that if
 * an exception is thrown at any point during the lifetime of the exception guard, it will be rolled back automatically.
 * When the exception guard is done, one must mark it as being complete, so it isn't rolled back when the exception
 * guard is destroyed.
 *
 * Exception guards are not default constructible, they cannot be copied or assigned to, but they can be moved around
 * for convenience.
 *
 * @tparam Rollback
 */
template <typename Rollback>
struct exception_guard {
    exception_guard() = delete;
    exception_guard(const exception_guard& other) = delete;
    exception_guard& operator=(const exception_guard& other) = delete;
    exception_guard& operator=(exception_guard&& other) = delete;

    constexpr exception_guard(Rollback&& rollback)
        : rollback_{std::move(rollback)}
        , complete_{false} {}

    constexpr exception_guard(exception_guard&& other) noexcept
        : rollback_{std::move(other.rollback_)}
        , complete_{std::exchange(other.complete_, true)} {}

    constexpr void complete() { complete_ = true; }

    constexpr ~exception_guard() {
        if (!complete_) {
            rollback_();
        }
    }

    Rollback rollback_;
    bool complete_;
};