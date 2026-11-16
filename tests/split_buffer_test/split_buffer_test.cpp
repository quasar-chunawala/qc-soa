#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <soa/construction_transaction.h>
#include <soa/split_buffer.h>
#include <soa/utils.h>
#include <stdexcept>
#include <type_traits>

namespace {
template <std::size_t Size, std::size_t Align>
struct alignas(Align) Blob {
    std::byte bytes[Size];
};

struct Vec3 {
    double x_;
    double y_;
    double z_;
};

// Members are ordered from largest to smallest
struct Particle {
    Vec3 position;
    Vec3 velocity;
    float lifetime;
    std::uint8_t color[4];
    std::uint16_t texture_id;
    bool alive;
};

struct Mixed {
    Blob<1, 1> a;
    Blob<16, 16> b;
    Blob<3, 1> c;
    Blob<8, 8> d;
    Blob<2, 2> e;
    Blob<4, 4> f;
};

struct ObjectTracker {
    static inline int alive_{0}, copies_{0}, moves_{0}, throw_countdown_{-1}, dtor_count_{0};

    enum class ThrowTrigger : uint8_t { DETERMINISTIC = 0, RANDOM = 1 };

    int value_;
    static inline ThrowTrigger throw_trigger_{ThrowTrigger::DETERMINISTIC};

    static void maybe_throw(std::string error_msg) {
        std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> uniform_int_dist(0, 1);
        if (throw_trigger_ == ThrowTrigger::DETERMINISTIC) {
            if (throw_countdown_-- == 0)
                throw std::runtime_error(error_msg);
        }

        if (throw_trigger_ == ThrowTrigger::RANDOM) {
            if (uniform_int_dist(generator)) {
                throw std::runtime_error(error_msg);
            }
        }
    }

    explicit ObjectTracker(int v) : value_{v} {
        maybe_throw("Runtime exception in ObjectTracker(int)!");
        ++alive_;
    }

    ObjectTracker(const ObjectTracker& other) : value_{other.value_} {
        maybe_throw("Runtime exception in copy ctor!");
        ++copies_;
        ++alive_;
    }

    ObjectTracker(ObjectTracker&& other) noexcept : value_{other.value_} {
        other.value_ = -1;
        ++alive_;
        ++moves_;
    }

    ~ObjectTracker() {
        --alive_;
        ++dtor_count_;
    }
    bool operator==(const ObjectTracker& other) const = default;
    static void reset() {
        alive_ = copies_ = moves_ = dtor_count_ = 0;
        throw_countdown_ = -1;
    }
};

struct ObjectTrackerNonMoveConstructible : ObjectTracker {
    ObjectTrackerNonMoveConstructible(ObjectTrackerNonMoveConstructible&&) = delete;
    ObjectTrackerNonMoveConstructible& operator=(ObjectTrackerNonMoveConstructible&&) = delete;
    ObjectTrackerNonMoveConstructible(int v) : ObjectTracker(v) {}
};

struct ObjectsCollection {
    ObjectTracker a_{0};
    ObjectTracker b_{0};

    ObjectsCollection(ObjectTracker a, ObjectTracker b) : a_(a), b_(b) {}
};

static std::mt19937 gen(42);
std::uniform_real_distribution<double> dist(0.0, 1.0);

} // namespace
TEST(SoaSplitBufferUtilsTest, MemberLayoutsOfVec3D) {
    auto layouts = soa::detail::member_layouts<Vec3>;
    for (auto i{0}; i < 3; ++i) {
        EXPECT_EQ(layouts[i].size, sizeof(double));
        EXPECT_EQ(layouts[i].alignment, alignof(double));
    }
}

TEST(SoaSplitBufferUtilsTest, MemberOffsetsOfParticle) {
    auto layouts = soa::detail::member_layouts<Particle>;
    EXPECT_EQ(layouts[0].size, sizeof(double) * 3);
    EXPECT_EQ(layouts[1].size, sizeof(double) * 3);
    EXPECT_EQ(layouts[2].size, sizeof(float));
    EXPECT_EQ(layouts[3].size, sizeof(uint8_t) * 4);
    EXPECT_EQ(layouts[4].size, sizeof(uint16_t));
    EXPECT_EQ(layouts[5].size, sizeof(bool));

    // Since the members of Particle are ordered from largest to smallest
    // we don't expect any padding.
    auto member_offsets = soa::detail::calculate_member_offsets<Particle>(100);
    size_t allocation_offset{0};
    for (auto i{0}; i < 6; ++i) {
        EXPECT_EQ(member_offsets[i].offset, allocation_offset);
        EXPECT_EQ(member_offsets[i].trailing_padding_bytes, 0);
        allocation_offset += layouts[i].size * 100;
    }
}

TEST(SoaSplitBufferUtilsTest, MemberOffsetsOfMixed) {
    // Capacity 7:
    //   a    0 ..   7  -> pad 9 to reach 16 (b is 16-aligned)
    //   b   16 .. 128
    //   c  128 .. 149  -> pad 3 to reach 152 (d is 8-aligned)
    //   d  152 .. 208
    //   e  208 .. 222  -> pad 2 to reach 224 (f is 4-aligned)
    //   f  224 .. 252
    auto member_offsets = soa::detail::calculate_member_offsets<Mixed>(7);
    EXPECT_EQ(member_offsets[0].alignment, 1);
    EXPECT_EQ(member_offsets[0].size, 1);
    EXPECT_EQ(member_offsets[0].offset, 0);
    EXPECT_EQ(member_offsets[0].trailing_padding_bytes, 9);

    EXPECT_EQ(member_offsets[1].alignment, 16);
    EXPECT_EQ(member_offsets[1].size, 16);
    EXPECT_EQ(member_offsets[1].offset, 16);
    EXPECT_EQ(member_offsets[1].trailing_padding_bytes, 0);

    EXPECT_EQ(member_offsets[2].alignment, 1);
    EXPECT_EQ(member_offsets[2].size, 3);
    EXPECT_EQ(member_offsets[2].offset, 128);
    EXPECT_EQ(member_offsets[2].trailing_padding_bytes, 3);

    EXPECT_EQ(member_offsets[3].alignment, 8);
    EXPECT_EQ(member_offsets[3].size, 8);
    EXPECT_EQ(member_offsets[3].offset, 152);
    EXPECT_EQ(member_offsets[3].trailing_padding_bytes, 0);

    EXPECT_EQ(member_offsets[4].alignment, 2);
    EXPECT_EQ(member_offsets[4].size, 2);
    EXPECT_EQ(member_offsets[4].offset, 208);
    EXPECT_EQ(member_offsets[4].trailing_padding_bytes, 2);

    EXPECT_EQ(member_offsets[5].alignment, 4);
    EXPECT_EQ(member_offsets[5].size, 4);
    EXPECT_EQ(member_offsets[5].offset, 224);
    EXPECT_EQ(member_offsets[5].trailing_padding_bytes, 0);

    EXPECT_EQ(soa::detail::calculate_allocation_size<Mixed>(7), 252);
}

TEST(SoaSplitBufferTest, DefaultConstructor) {
    struct Point2D {
        double x_;
        double y_;
    };
    soa::detail::split_buffer<Point2D> buffer;
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), 0);
}

TEST(SoaSplitBufferTest, CapacityAndBeginOffsetConstructor) {
    soa::detail::split_buffer<Vec3> split_buf(1000, 250);
    EXPECT_EQ(split_buf.size(), 0);
    EXPECT_EQ(split_buf.capacity(), 1000);
    EXPECT_EQ(split_buf.front_spare(), 250);
}

TEST(SoaSplitBufferTest, ConstructAtEndWithSizeTest) {
    soa::detail::split_buffer<Vec3> buf(1000, 250);
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.capacity(), 1000);
    EXPECT_EQ(buf.front_spare(), 250);

    Vec3 i_hat{.x_ = 1.0, .y_ = 0.0, .z_ = 0.0};
    buf.copy_construct_at_end_with_size(1, i_hat);
    EXPECT_EQ(buf.size(), 1);
    EXPECT_EQ(buf[0], i_hat);

    Vec3 j_hat{.x_ = 0.0, .y_ = 1.0, .z_ = 0.0};
    buf.copy_construct_at_end_with_size(1, j_hat);
    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf[1], j_hat);

    Vec3 k_hat{.x_ = 0.0, .y_ = 0.0, .z_ = 1.0};
    buf.copy_construct_at_end_with_size(1, k_hat);
    EXPECT_EQ(buf.size(), 3);
    EXPECT_EQ(buf[2], k_hat);
}

TEST(SoaSplitBufferTest, CopyConstructAtEndWithSentinelTest) {
    soa::detail::split_buffer<Vec3> split_buf(1000, 250);
    EXPECT_EQ(split_buf.size(), 0);
    EXPECT_EQ(split_buf.capacity(), 1000);
    EXPECT_EQ(split_buf.front_spare(), 250);
    EXPECT_EQ(split_buf.back_spare(), 750);

    std::vector<Vec3> vec;
    for (auto i{0}; i < 500; ++i) {
        vec.push_back(Vec3{.x_ = 1.0 * i, .y_ = 2.0 * i, .z_ = 3.0 * i});
    }

    split_buf.construct_at_end_with_sentinel(vec.begin(), vec.end());
    EXPECT_EQ(split_buf.size(), 500);
    EXPECT_EQ(split_buf.capacity(), 1000);
    EXPECT_EQ(split_buf.front_spare(), 250);
    EXPECT_EQ(split_buf.back_spare(), 250);

    for (auto i{0}; i < 500; ++i) {
        EXPECT_EQ(split_buf[i], vec[i]);
    }
}

TEST(SoaSplitBufferTest, ConstructAtEndFromRangeTest) {
    soa::detail::split_buffer<Vec3> split_buf(1000, 250);
    EXPECT_EQ(split_buf.size(), 0);
    EXPECT_EQ(split_buf.capacity(), 1000);

    std::vector<Vec3> points;
    points.reserve(100);
    for (auto i{0uz}; i < 100; ++i) {
        points.push_back(Vec3{.x_ = 1.0 * i, .y_ = 2.0 * i, .z_ = 3.0 * i});
    }

    split_buf.copy_construct_at_end_with_size(points.begin(), points.end());
    EXPECT_EQ(split_buf.size(), 100);

    for (auto i{0uz}; i < 100; ++i) {
        EXPECT_EQ(split_buf[i], points[i]);
    }
}

/**
 * @brief A deterministic test for emplace_back(Args&&) where the container has both
 * front and back spare capacity.
 */
TEST(SoaSplitBufferTest, EmplaceBackWitHFrontAndBackSpareTest) {
    soa::detail::split_buffer<Vec3> split_buf(1000, 250);
    EXPECT_EQ(split_buf.size(), 0);
    EXPECT_EQ(split_buf.capacity(), 1000);

    split_buf.emplace_back(1.0, -1.0, 1.0);
    EXPECT_EQ(split_buf.size(), 1);
    EXPECT_EQ(split_buf[0], Vec3(1.0, -1.0, 1.0));
}

/**
 * @brief A deterministic test for emplace_back(Args&&) where the container has no back spare
 * spare capacity.
 */
TEST(SoaSplitBufferTest, EmplaceBackNoBackSpareCapacityTest) {
    soa::detail::split_buffer<Vec3> split_buf(5, 4);
    EXPECT_EQ(split_buf.size(), 0);
    EXPECT_EQ(split_buf.capacity(), 5);
    EXPECT_EQ(split_buf.back_spare(), 1);

    Vec3 v = Vec3{.x_ = dist(gen), .y_ = dist(gen), .z_ = dist(gen)};
    split_buf.copy_construct_at_end_with_size(1, v);
    EXPECT_EQ(split_buf.size(), 1);
    EXPECT_EQ(split_buf.back_spare(), 0);
    EXPECT_EQ(split_buf.front_spare(), 4);

    split_buf.emplace_back(1.0, -1.0, 1.0);
    EXPECT_EQ(split_buf.size(), 2);
    EXPECT_EQ(split_buf[0], v);
    EXPECT_EQ(split_buf[1], Vec3(1.0, -1.0, 1.0));
}

TEST(SoaSplitBufferTest, EmplaceBackNoSpareCapacityTest) {
    soa::detail::split_buffer<Vec3> split_buf;
    EXPECT_EQ(split_buf.size(), 0);
    EXPECT_EQ(split_buf.capacity(), 0);

    std::vector<Vec3> vec;
    for (auto i{0uz}; i < 128; ++i) {
        vec.push_back(Vec3(1.0 * i, 2.0 * i, 3.0 * i));
    }

    for (auto i{0uz}; i < 128; ++i) {
        split_buf.emplace_back(vec[i].x_, vec[i].y_, vec[i].z_);
    }

    for (auto i{0uz}; i < 128; ++i) {
        EXPECT_EQ(split_buf[i], vec[i]);
    }

    EXPECT_EQ(split_buf.size(), 128);
}

/**
 * @brief Tests emplace_back(Args&&) when there is no capacity at the back,
 * but there is capacity at the front.
 *
 */
TEST(SoaSplitBufferTest, EmplaceBackNoBackSpareObjectTrackingTest) {
    std::vector<ObjectsCollection> vec;
    for (auto i{0}; i < 5; ++i) {
        vec.push_back(ObjectsCollection(ObjectTracker(i), ObjectTracker(2 * i)));
    }

    // Reset count
    ObjectTracker::reset();

    soa::detail::split_buffer<ObjectsCollection> split_buf(10, 6);
    EXPECT_EQ(split_buf.capacity(), 10);
    EXPECT_EQ(split_buf.size(), 0);
    EXPECT_EQ(split_buf.front_spare(), 6);
    EXPECT_EQ(split_buf.back_spare(), 4);

    split_buf.construct_at_end_with_sentinel(vec.begin(), vec.begin() + 4);
    EXPECT_EQ(split_buf.size(), 4);
    EXPECT_GT(split_buf.front_spare(), 0);
    EXPECT_EQ(split_buf.back_spare(), 0);
    EXPECT_EQ(ObjectTracker::alive_, 8);
    EXPECT_EQ(ObjectTracker::copies_, 8);
    EXPECT_EQ(ObjectTracker::moves_, 0);

    // As back_spare()==0, issuing an emplace_back will force the 4 elements at rear-end to be shifted to the front of
    // the buffer. Each element consists of two member objects that are move constructed, so this contributes 4 x 2 = 8
    // moves. If no exception is thrown when shifting elements to the left, the moved-from elements are destructed,
    // which sets dtor_count = 8. Finally, the 2 arguments to emplace_back are temporaries, so these are move
    // constructed in place in the buffer and the temporary objects themselves are destructed. Thus, we expect
    // `ObjectTracker::moves_=10` and `ObjectTracker::dtor_count_ = 10`.

    split_buf.emplace_back(ObjectTracker(4), ObjectTracker(8));
    EXPECT_EQ(split_buf.size(), 5);
    EXPECT_EQ(ObjectTracker::alive_, 10);

    static_assert(std::is_nothrow_move_constructible_v<ObjectsCollection>);
    EXPECT_EQ(ObjectTracker::moves_, 10);
    EXPECT_EQ(ObjectTracker::dtor_count_, 10);

    for (auto i{0}; i < 5; ++i) {
        EXPECT_EQ(vec[i], split_buf[i]);
    }
}

/**
 * @brief Tests emplace_back(Args&&) when there is no spare capacity in the buffer.
 *
 */
TEST(SoaSplitBufferTest, EmplaceBackNoSpareCapacityObjectTrackingTest) {
    std::vector<ObjectsCollection> vec;
    for (auto i{0}; i < 11; ++i) {
        vec.push_back(ObjectsCollection(ObjectTracker(i), ObjectTracker(2 * i)));
    }

    // Reset count
    ObjectTracker::reset();

    soa::detail::split_buffer<ObjectsCollection> split_buf(10, 0);
    EXPECT_EQ(split_buf.capacity(), 10);
    EXPECT_EQ(split_buf.size(), 0);

    split_buf.construct_at_end_with_sentinel(vec.begin(), vec.begin() + 10);
    EXPECT_EQ(split_buf.size(), 10);
    EXPECT_EQ(split_buf.size(), split_buf.capacity());
    EXPECT_EQ(ObjectTracker::alive_, 20);

    split_buf.emplace_back(ObjectTracker(10), ObjectTracker(20));
    EXPECT_EQ(split_buf.size(), 11);
    EXPECT_EQ(ObjectTracker::alive_, 22);

    // As there is no spare capacity in the buffer(size() == capacity()), a new allocation is requested,
    // the existing elements are moved from the old buffer to the new allocation. Each element consists of two member
    // objects that are move constructed, so this relocation results in 10 x 2 = 20 moves. If no exception is thrown,
    // when relocating the elements, the pointers to the old buffer and the new allocation are swapped and scope is
    // exited. When the old buffer goes out of scope, its destructor clears the buffer, destructs the contents of the
    // old buffer, so the dtor_count equals 20. Lastly, the 2 arguments to emplace_back are temporaries, which are move
    // constructed in place in the buffer and the temporaries are destroyed. Thus, when emplace_back completes, we
    // expect `ObjectTracker::moves_=22` and `ObjectTracker::dtor_count_ = 22`.
    static_assert(std::is_nothrow_move_constructible_v<ObjectsCollection>);
    EXPECT_EQ(ObjectTracker::moves_, 22);
    EXPECT_EQ(ObjectTracker::dtor_count_, 22);

    for (auto i{0uz}; i < 11; ++i) {
        EXPECT_EQ(vec[i], split_buf[i]);
    }
}

TEST(SoaSplitBufferTest, ThrowInElementConstructionTransactionTest) {

    ObjectTracker* raw_mem = static_cast<ObjectTracker*>(operator new(5 * sizeof(ObjectTracker)));

    std::vector<ObjectTracker> vec;
    for (auto i{0}; i < 5; ++i) {
        vec.push_back(ObjectTracker(i + 1));
    }

    auto ptrs_tuple = std::tuple{raw_mem, raw_mem + 1, raw_mem + 2, raw_mem + 3, raw_mem + 4};

    {
        ObjectTracker::reset();
        ObjectTracker::throw_countdown_ = 2;

        soa::detail::ElementConstructionTransaction elementConstructionTxn(ptrs_tuple, std::allocator<ObjectTracker>{});

        EXPECT_EQ(ObjectTracker::copies_, 0);

        // The ElementConstructionTransaction is an RAII wrapper that offers strong exception guarantee. It is
        // initialized with a tuple of ptrs to memory locations. The .construct member function when invoked on
        // `ElementConstructionTransaction` instance accepts a pack of arguments and copy/move constructs values in
        // place at the range of address supplied earlier. The ObjectTracker is configured to throw after 2 invocations
        // of the copy constructor. So, the below construct call copies vec[0], vec[1] and fails at vec[2]. Being an
        // RAII wrapper, its destructor performs rollback and destroys 2 elements. Hence, it is strong-exception safe.
        // So, we expect, that when `elementConstructionTransaction` goes out of scope, `ObjectTracker::copies_ = 2` and
        // `ObjectTracker::dtor_count_ = 2`.
        EXPECT_THROW(elementConstructionTxn.construct(vec[0], vec[1], vec[2], vec[3], vec[4]), std::runtime_error);
        EXPECT_EQ(ObjectTracker::copies_, 2);
        EXPECT_EQ(elementConstructionTxn.num_elements_constructed, 2u);
        EXPECT_EQ(ObjectTracker::dtor_count_, 0); // rollback happens in the destructor, not in construct()
    }

    // The transaction went out of scope with only 2 of 5 members built, so it destroyed those 2.
    EXPECT_EQ(ObjectTracker::dtor_count_, 2);
    EXPECT_EQ(ObjectTracker::alive_, 0);
}

// TODO: Add tests for emplace_back when the constructor throws
TEST(SoaSplitBufferTest, ThrowInEmplaceBackNoBackSpareCapacityTest) {}