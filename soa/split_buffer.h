#ifndef SOA_SPLIT_BUFFER_H
#define SOA_SPLIT_BUFFER_H

#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <print>
#include <ranges>
#include <tuple>
#include <type_traits>

#include <contracts/contract_assert.h>
#include <soa/utils.h>
#include <utility>

namespace soa::detail {
template <typename T, typename Allocator>
struct split_buffer;

namespace split_buffer_internal {
/**
 * @brief  ConstructionTransaction is an RAII wrapper used for book-keeping. If the client iteratively
 * constructs a sequence of T object in the split_buffer, and if any of constructions throw an exception,
 * ConstructionTransaction ensures that the split_buffer stays in a valid state.
 * It provides basic exception guarantee, so no resources are leaked, and the split_buffer's invariants
 * are intact.
 *
 * @tparam T
 * @tparam Alloc
 * @tparam Iterator
 */
template <typename T, typename Alloc, typename Iterator>
struct ConstructionTransaction {
    ConstructionTransaction(split_buffer<T, Alloc>* parent, Iterator pos, Iterator end);

    ConstructionTransaction(split_buffer<T, Alloc>* parent, Iterator pos, std::size_t n);

    ~ConstructionTransaction();
    split_buffer<T, Alloc>* parent_;
    Iterator pos_;
    const Iterator end_;

    friend class split_buffer<T, Alloc>;
};

template <typename T, typename Alloc, typename Iterator>
ConstructionTransaction<T, Alloc, Iterator>::ConstructionTransaction(split_buffer<T, Alloc>* parent, Iterator pos,
                                                                     Iterator end)
    : parent_{parent}
    , pos_{pos}
    , end_{end} {}

template <typename T, typename Alloc, typename Iterator>
ConstructionTransaction<T, Alloc, Iterator>::ConstructionTransaction(split_buffer<T, Alloc>* parent, Iterator pos,
                                                                     std::size_t n)
    : parent_{parent}
    , pos_{pos}
    , end_{pos + n} {}

template <typename T, typename Alloc, typename Iterator>
ConstructionTransaction<T, Alloc, Iterator>::~ConstructionTransaction() {
    parent_->set_sentinel(pos_);
}

/**
 * @brief ElementConstructionTransaction is an RAII class that guarantees
 * strong exception safety, when constructing an element in soa_split_buffer container.
 *
 */
template <typename Allocator, typename... MemberTypes>
struct ElementConstructionTransaction {
    // std::tuple<MemberTypes&&...> args_;
    std::tuple<std::remove_cvref_t<MemberTypes>*...> ptrs_;
    size_t num_elements_constructed{0};
    [[no_unique_address]] Allocator allocator_;

    constexpr ElementConstructionTransaction(std::tuple<MemberTypes*...> ptrs, const Allocator& allocator)
        : ptrs_{ptrs}
        , allocator_{allocator} {}

    ElementConstructionTransaction(const ElementConstructionTransaction&) = delete;
    ElementConstructionTransaction& operator=(const ElementConstructionTransaction&) = delete;

    /// Destroys the members constructed so far, in reverse order of construction.
    constexpr void destruct() {
        [&]<size_t... Indices>(std::index_sequence<Indices...>) {
            (((Indices < num_elements_constructed) ? (std::destroy_at(std::get<Indices>(ptrs_)), 0) : 0), ...);
        }(std::make_index_sequence<sizeof...(MemberTypes)>{});
        num_elements_constructed = 0;
    }

    template <typename Arg, typename Member>
    constexpr void construct_impl(Arg&& arg, Member* ptr) {
        std::construct_at(ptr, std::forward<Arg>(arg));
        ++num_elements_constructed;
    }

    constexpr ~ElementConstructionTransaction() {
        if (num_elements_constructed < sizeof...(MemberTypes)) {
            destruct();
        }
    }

    template <typename... ConstructorArgs>
    constexpr void construct(ConstructorArgs&&... args) {
        template for (constexpr size_t I : std::views::iota(0uz, sizeof...(ConstructorArgs))) {
            construct_impl(std::forward<ConstructorArgs...[I]>((args...[I])), std::get<I>(ptrs_));
        }
    }
};
} // namespace split_buffer_internal

} // namespace soa::detail
namespace soa::detail {
template <typename Iterator>
struct is_move_iterator {
    using type = std::false_type;
};

template <typename Iterator>
struct is_move_iterator<std::move_iterator<Iterator>> {
    using type = std::true_type;
};

template <typename Iterator>
using is_move_iterator_t = is_move_iterator<Iterator>::type;

/**
 * @brief An internal storage for the soa_vector.
 *
 * @details @a soa_split_buffer is a contiguous array data structure. It may
 * hold spare capacity at both ends of the buffer. This allows a
 * soa_split_buffer to grow from both the front and the back without relocating
 * its contents until it runs out of room.
 *
 * The contents of the split_buffer are stored in a contiguous buffer
 * allocated on the heap. For an aggregate type `T{m_0, m_1, m_2}`, each member
 * is stored in its own sub-array, delimited by the pointers shown below (`o`
 * marks uninitialized memory and `x` marks valid objects).
 *
 * @verbatim
 * |ooooooooooooooxxxxxxxxooooooo|ooooooooooooxxxxxxxxxxxxxxxoooooooooooo|
 *  ^             ^       ^       ^           ^              ^           ^
 *  front_cap_0   |       end_0   |           begin_1        | back_cap_1
 *                begin_0         back_cap_0 = front_cap_1   end_1
 *  @endverbatim
 *
 */

template <typename T, typename Allocator = std::allocator<T>>
struct split_buffer {
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    template <typename U, typename Alloc, typename Iterator>
    using ConstructionTransaction = split_buffer_internal::ConstructionTransaction<U, Alloc, Iterator>;
    template <typename Alloc, typename... MemberTypes>
    using ElementConstructionTransaction = split_buffer_internal::ElementConstructionTransaction<Alloc, MemberTypes...>;

    struct Pointers;
    struct ConstPointers;
    struct ReferencesBase;
    struct ConstReferencesBase;
    struct References;
    struct ConstReferences;

    consteval {
        auto data_members = std::define_static_array(get_all_fields(^^T));
        define_aggregate(^^Pointers, data_members | std::views::transform([](std::meta::info member) {
                                         return data_member_spec(add_pointer(type_of(member)),
                                                                 {.name = identifier_of(member)});
                                     }));
        define_aggregate(^^ConstPointers, data_members | std::views::transform([](std::meta::info member) {
                                              return data_member_spec(add_pointer(add_const(type_of(member))),
                                                                      {.name = identifier_of(member)});
                                          }));
        define_aggregate(^^ReferencesBase, data_members | std::views::transform([](std::meta::info member) {
                                               return data_member_spec(add_lvalue_reference(type_of(member)),
                                                                       {.name = identifier_of(member)});
                                           }));
        define_aggregate(^^ConstReferencesBase,
                         data_members | std::views::transform([](std::meta::info member) {
                             return data_member_spec(add_lvalue_reference(add_const(type_of(member))),
                                                     {.name = identifier_of(member)});
                         }));
    }

    static constexpr auto ptr_members = define_static_array(get_all_fields(^^Pointers));
    static constexpr auto const_ptr_members = define_static_array(get_all_fields(^^ConstPointers));
    static constexpr auto members = define_static_array(get_all_fields(^^T));
    static constexpr auto ref_members = define_static_array(get_all_fields(^^ReferencesBase));
    static constexpr auto const_ref_members = define_static_array(get_all_fields(^^ConstReferencesBase));

    /**
     * @brief References is a modifiable view into an element of the split_buffer.
     * @details References is a proxy for a `T` object. If you access elements of the split_buffer container
     * using an index, e.g. split_buf[2], or dereference an iterator e.g. *it, these operations return an object of type
     * `References` rather than `T&`. The members of this struct are references to the members of the value_type. So, a
     * `References` object behaves like a `T&`.
     */
    struct References : ReferencesBase {

        /**
         * @brief Construct a new References object from a pack of reference to args.
         *
         * @tparam Args
         * @param args
         */
        template <typename... Args>
        References(Args&... args)
            : ReferencesBase{args...} {}

        /**
         * @brief Mutator method, used when user tries to copy-assign to an element of the buffer.
         *
         * @param value
         * @return References&
         */
        References& operator=(const T& value) {
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                this->[:ref_members[I]:] = value.[:members[I]:];
            }
            return *this;
        }

        /**
         * @brief Mutator method, used when user tries to move-assign to an element of the buffer.
         *
         * @param value
         * @return References&
         */
        References& operator=(T&& value) {
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                this->[:ref_members[I]:] = std::move(value.[:members[I]:]);
            }
            return *this;
        }

        /**
         * @brief Copy-assignment operator.
         *
         * @param value
         * @return References&
         */
        References& operator=(const References& value) {
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                this->[:ref_members[I]:] = (value.[:ref_members[I]:]);
            }
            return *this;
        }

        /**
         * @brief Move-assignment operator.
         *
         * @param value
         * @return References&
         */
        References& operator=(References&& value) {
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                this->[:ref_members[I]:] = std::move(value.[:ref_members[I]:]);
            }
            return *this;
        }

        /**
         * @brief Arrow operator.
         *
         * @return References*
         */
        References* operator->() { return this; }

        /**
         * @brief Equality comparison with a T. Performs a member-wise comparison.
         *
         * @param value
         * @return true
         * @return false
         */
        bool operator==(const T& value) const {
            bool isEqual{true};
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                isEqual &= (this->[:ref_members[I]:] == value.[:members[I]:]);
            }
            return isEqual;
        }

        /**
         * @brief Equality comparison with another proxy object. Performs a member-wise comparison.
         *
         * @param value
         * @return true
         * @return false
         */
        bool operator==(const References& value) const {
            bool isEqual{true};
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                isEqual &= (this->[:ref_members[I]:] == value.[:ref_members[I]:]);
            }
            return isEqual;
        }
    };

    struct ConstReferences : ConstReferencesBase {

        template <typename... Args>
        ConstReferences(Args&... args)
            : ConstReferencesBase{args...} {}

        ConstReferences(const References& other) {
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                this->[:const_ref_members[I]:] == other.[:ref_members[I]:];
            }
        }

        ConstReferences* operator->() { return this; }

        /**
         * @brief Comparison to a T object.
         *
         * @param value
         * @return true
         * @return false
         */
        bool operator==(const T& value) const {
            bool isEqual{true};
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                isEqual &= (this->[:const_ref_members[I]:] == value.[:members[I]:]);
            }
            return isEqual;
        }

        /**
         * @brief Comparison to another proxy object.
         *
         * @param other
         * @return true
         * @return false
         */
        bool operator==(ConstReferences other) const {
            bool isEqual{true};
            template for (constexpr size_t I : std::views::iota(0uz, members.size())) {
                isEqual &= (this->[:const_ref_members[I]:] == other.[:const_ref_members[I]:]);
            }
            return isEqual;
        }
    };

    /**
     * @brief The unit of allocation for the buffer's storage. One block is `alignof(T)` bytes and is aligned to
     * `alignof(T)`.
     *
     * @details The split_buffer stores members of `T` in separate sub-arrays inside a single contiguous blob of memory
     * allocated on the heap. This blob is requested as raw bytes, but it must start at an address that is suitably
     * aligned for every member type. A standard std::byte allocation does not guarantee this.
     *
     * `StorageBlock` solves this by being a byte-array that is aligned like `T`. The user's custom allocator is
     * rebound to `StorageBlock`. Therefore, the rebound allocator returns storage aligned for `StorageBlock`, so
     * the memory thus obtained is at an address which is a multiple of `alignof(T)`. That is enough for every
     * data-member of `T`, because an aggregate's alignment is atleast that of each of its members.
     *
     * Each sub-array's offset within the buffer is computed separately by `calculate_member_offsets`.
     */
    struct alignas(T) StorageBlock {
        // Raw storage of exactly `alignof(T)` bytes, so that the blocks tile
        // contiguously without any padding.
        std::byte byte_array[alignof(T)];
    };

    Pointers pointers_{};
    /**
     * @brief The start of the allocation.
     */
    StorageBlock* raw_memory_{nullptr};
    std::size_t begin_offset_{0};
    std::size_t size_{0};
    std::size_t capacity_{0};
    [[no_unique_address]] Allocator alloc_;

    using tuple_pointers_t = [:std::meta::substitute(^^std::tuple, ptr_members |
                                                                       std::views::transform(std::meta::type_of) |
                                                                       std::ranges::to<std::vector>()):];
    using tuple_members_t = [:std::meta::substitute(^^std::tuple, members | std::views::transform(std::meta::type_of) |
                                                                      std::ranges::to<std::vector>()):];
    using tuple_references_t = [:std::meta::substitute(^^std::tuple,
                                                       members | std::views::transform(std::meta::type_of) |
                                                           std::views::transform(std::meta::add_lvalue_reference) |
                                                           std::ranges::to<std::vector>()):];
    template <bool IsConst>
    struct basic_iterator {
        using ptrs_t = std::conditional_t<IsConst, ConstPointers, Pointers>;
        using ref_t = std::conditional_t<IsConst, ConstReferences, References>;
        using iterator_category = std::contiguous_iterator_tag;
        using iterator_type = ptrs_t;
        using value_type = ref_t;
        using pointer = ptrs_t;
        using difference_type = std::ptrdiff_t;

        constexpr basic_iterator operator++() {
            auto [... ptrs] = ptrs_;
            ptrs_ = ptrs_t{(ptrs + 1)...};
            return *this;
        }

        constexpr basic_iterator operator++(int) {
            basic_iterator temp = *this;
            ++(*this);
            return temp;
        }

        constexpr basic_iterator operator--() {
            auto [... ptrs] = ptrs_;
            ptrs_ = ptrs_t{(ptrs - 1)...};
            return *this;
        }

        constexpr basic_iterator operator--(int) {
            basic_iterator temp = *this;
            --(*this);
            return temp;
        }

        constexpr basic_iterator operator+(std::ptrdiff_t n) {
            auto [... ptrs] = ptrs_;
            return basic_iterator(ptrs_t{(ptrs + n)...});
        }

        constexpr basic_iterator& operator+=(std::ptrdiff_t n) {
            auto [... ptrs] = ptrs_;
            *this = basic_iterator(ptrs_t{(ptrs + n)...});
            return *this;
        }

        constexpr basic_iterator operator-(std::ptrdiff_t n) {
            auto [... ptrs] = ptrs_;
            // auto [... result_ptrs] = ((ptrs - n), ...);
            return basic_iterator(ptrs_t{(ptrs - n)...});
        }

        constexpr basic_iterator& operator-=(std::ptrdiff_t n) {
            auto [... ptrs] = ptrs_;
            *this = basic_iterator(ptrs_t{(ptrs - n)...});
            return *this;
        }

        friend constexpr basic_iterator operator+(std::ptrdiff_t n, const basic_iterator& rhs) { return rhs + n; }

        friend constexpr basic_iterator operator-(std::ptrdiff_t n, const basic_iterator& rhs) { return rhs - n; }

        constexpr std::ptrdiff_t operator-(const basic_iterator& rhs) {
            auto [lhs_first, ... ptrs1] = ptrs_;
            auto [rhs_first, ... ptrs2] = rhs.ptrs_;
            return lhs_first - rhs_first;
        }

        constexpr ref_t operator*() {
            auto [... ptrs] = ptrs_;
            return ref_t{(*ptrs)...};
        }

        constexpr const ref_t operator*() const {
            auto [... ptrs] = ptrs_;
            return ref_t{(*ptrs)...};
        }

        constexpr ref_t operator[](std::size_t idx) { return ref_t{*(*this + idx)}; }

        constexpr const ref_t operator[](std::size_t idx) const { return ref_t{*(*this + idx)}; }

        constexpr ref_t* operator->() {
            auto [... ptrs] = ptrs_;
            ref_t ref = ref_t((*ptrs)...);
            return ref;
        }

        constexpr const ref_t* operator->() const {
            auto [... ptrs] = ptrs_;
            ref_t ref = ref_t((*ptrs)...);
            return ref;
        }

        constexpr bool operator==(const basic_iterator& rhs) const {
            auto [... lhs_ptrs] = ptrs_;
            auto [... rhs_ptrs] = rhs.ptrs_;
            return ((lhs_ptrs == rhs_ptrs) && ...);
        }

        constexpr bool operator==(std::nullptr_t) const {
            auto [... ptrs] = ptrs_;
            return ((ptrs == nullptr) && ...);
        }

        constexpr friend bool operator<(const basic_iterator& lhs, const basic_iterator& rhs) {
            auto [... lhs_ptrs] = lhs.ptrs_;
            auto [... rhs_ptrs] = rhs.ptrs_;
            return ((lhs_ptrs < rhs_ptrs) && ...);
        }

        constexpr friend bool operator>(const basic_iterator& lhs, const basic_iterator& rhs) {
            auto [... lhs_ptrs] = lhs.ptrs_;
            auto [... rhs_ptrs] = rhs.ptrs_;
            return ((lhs_ptrs > rhs_ptrs) && ...);
        }

        constexpr friend bool operator>=(const basic_iterator& lhs, const basic_iterator& rhs) {
            return lhs > rhs || lhs == rhs;
        }

        constexpr friend bool operator<=(const basic_iterator& lhs, const basic_iterator& rhs) {
            return lhs < rhs || lhs == rhs;
        }

        constexpr basic_iterator& operator=(std::nullptr_t) {
            auto& [... ptrs] = ptrs_;
            ((ptrs = nullptr), ...);
        }

        constexpr ptrs_t& get() const { return ptrs_; }

        ptrs_t ptrs_;
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    constexpr iterator begin() {
        iterator front_cap(pointers_);
        return front_cap + begin_offset_;
    }

    constexpr const_iterator begin() const {
        const_iterator front_cap(pointers_);
        return front_cap + begin_offset_;
    }

    constexpr iterator end() {
        iterator front_cap(pointers_);
        return front_cap + begin_offset_ + size_;
    }

    constexpr const_iterator end() const {
        const_iterator front_cap(pointers_);
        return front_cap + begin_offset_ + size_;
    }

    /**
     * @brief Returns a modifiable view of a single member's values across all valid elements of
     * the container.
     *
     * @details Because the buffer stores each member of `T` in its own contiguous sub-array,
     * the values of one member of `T` can be directly exposed as a `std::span`. This method allows the user to specify
     * the member by its type.
     * @tparam FieldType The declared type of the member to view.
     * @return constexpr auto
     */
    template <typename FieldType>
    constexpr auto get_field_span() {
        auto [... ptrs] = pointers_;
        auto tuple_pointers = std::make_tuple(ptrs...);
        FieldType* front_cap_ptr = std::get<FieldType*>(tuple_pointers);
        return std::span(front_cap_ptr + begin_offset_, size_);
    }

    /**
     * @brief Accessor method that returns a read-only view of a single member's values across all valid elements stored
     * in the container.
     *
     * @tparam FieldType The declared type of the member to view.
     * @return constexpr auto
     */
    template <typename FieldType>
    constexpr auto get_field_span() const {
        auto [... ptrs] = pointers_;
        auto tuple_pointers = std::make_tuple(ptrs...);
        const FieldType* front_cap_ptr = std::get<FieldType*>(tuple_pointers);
        return std::span(front_cap_ptr + begin_offset_, size_);
    }

    /**
     * @brief Returns a modifiable view of a single member's values across all elements stored in the container.
     *
     * @details This method allows the user to specify the member by its index.
     * @tparam N The index of the member to view.
     * @return constexpr auto
     */
    template <std::size_t N>
    constexpr auto get_field_span() {
        auto [... ptrs] = pointers_;
        auto tuple_pointers = std::make_tuple(ptrs...);
        auto* front_cap_ptr = std::get<N>(tuple_pointers);
        return std::span(front_cap_ptr + begin_offset_, size_);
    }

    /**
     * @brief Returns a read-only view of a single member's values across all valid elements stored in the container.
     *
     * @tparam N The index of the member to view.
     * @return constexpr auto
     */
    template <std::size_t N>
    constexpr auto get_field_span() const {
        auto [... ptrs] = pointers_;
        auto tuple_pointers = std::make_tuple(ptrs...);
        const auto* front_cap_ptr = std::get<N>(tuple_pointers);
        return std::span(front_cap_ptr + begin_offset_, size_);
    }

    [[nodiscard]] constexpr std::size_t size() const { return size_; }

    constexpr void set_size(size_t new_size) { size_ = new_size; }

    [[nodiscard]] constexpr bool empty() const { return size_ == 0; }

    [[nodiscard]] constexpr std::size_t capacity() const { return capacity_; }

    [[nodiscard]] constexpr std::size_t front_spare() const { return begin_offset_; }

    constexpr void set_front_spare(size_t new_begin_offset) { begin_offset_ = new_begin_offset; }

    [[nodiscard]] constexpr std::size_t back_spare() const { return capacity_ - front_spare() - size_; }

    constexpr References front() {
        auto [... ptr] = pointers_;
        return References(*(ptr + begin_offset_)...);
    }

    constexpr ConstReferences front() const {
        auto [... ptr] = pointers_;
        return ConstReferences(*(ptr + begin_offset_)...);
    }
    constexpr References back() {
        auto [... ptr] = pointers_;
        return References(*(ptr + begin_offset_ + size_)...);
    }

    constexpr ConstReferences back() const {
        auto [... ptr] = pointers_;
        return ConstReferences(*(ptr + begin_offset_ + size_)...);
    }

    [[nodiscard]] constexpr std::size_t get_sentinel() const { return size_; }

    constexpr void set_sentinel(size_type size) { size_ = size; }

    constexpr void set_sentinel(iterator new_end) {
        auto [begin_ptr, ... begin_ptrs] = begin().ptrs_;
        auto [end_ptr, ... end_ptrs] = new_end.ptrs_;
        set_size(end_ptr - begin_ptr);
    }

    constexpr void set_capacity(size_type new_capacity) { capacity_ = new_capacity; }

    [[nodiscard]] constexpr std::size_t get_begin_offset() const { return begin_offset_; }

    constexpr void set_begin_offset(size_type new_begin_offset_) { begin_offset_ = new_begin_offset_; }

    constexpr void set_valid_range(size_type new_begin_offset, size_type new_size) {
        begin_offset_ = new_begin_offset;
        size_ = new_size;
    }

    constexpr void set_valid_range(iterator first, iterator last) {
        auto [begin_cap_ptr, ... begin_cap_ptrs] = pointers_;
        auto [first_ptr, ... first_ptrs] = first.ptrs_;
        auto [last_ptr, ... last_ptrs] = last.ptrs_;
        begin_offset_ = std::distance(begin_cap_ptr, first_ptr);
        size_ = std::distance(first_ptr, last_ptr);
    }

    /**
     * @brief Sets the split_buffer's raw_memory_ byte pointer to the starting
     * address of the allocated memory. The split_buffer's contigous storage is
     * divided into sub-arrays, one each for T's member values. pointers_ field
     * is initialized to the base addresses of each of the sub-arrays.
     *
     * @param raw_mem Starting address of the allocated memory on the heap.
     * @param capacity
     */
    constexpr void set_pointers(std::byte* raw_mem, size_type capacity) {
        raw_memory_ = reinterpret_cast<StorageBlock*>(raw_mem);
        const auto member_offsets = calculate_member_offsets<T>(capacity);
        std::size_t i = 0;
        template for (constexpr auto ptr_member : ptr_members) {
            pointers_.[:ptr_member:] = reinterpret_cast<[:type_of(ptr_member):]>(raw_mem + member_offsets[i++].offset);
        }
        set_capacity(capacity);
    }

    /**
     * @brief Constructs every member of a single T element, each at its own address.
     *
     * @details Each member is constructed in the declaration order from the matching value in `member_values`. If any
     * member's constructor throws, the members built so far are destroyed before the exception propogates. The target
     * storage is then left uninitialized, so this function either constructs the whole T element or nothing.
     * @tparam MemberTypes
     * @param tuple_ptrs A tuple of per-member addresses
     * @param member_values A pack of member values.
     */
    template <typename... MemberTypes>
    constexpr void construct_members_at(auto tuple_ptrs, MemberTypes&&... member_values) {
        ElementConstructionTransaction elementConstructionTransaction(tuple_ptrs, alloc_);
        elementConstructionTransaction.construct(std::forward<MemberTypes>(member_values)...);
    }

    template <typename... MemberTypes>
    constexpr void destruct_members_at(const std::tuple<MemberTypes*...>& member_ptrs) {
        template for (constexpr size_t FieldIdx : std::views::iota(0uz, sizeof...(MemberTypes))) {
            std::destroy_at(std::get<FieldIdx>(member_ptrs));
        }
    }

    constexpr void destruct_members_at(iterator pos) {
        auto [... ptrs] = pos.ptrs_;
        destruct_members_at(std::make_tuple(ptrs...));
    }

    /**
     * @brief Copy constructs n objects starting at end().
     *
     * @pre n > 0
     * @pre size() + n <= capacity()
     * @post size() = old_size + n
     * @post [i] == value for all i in [size() - n, size())
     */
    constexpr void construct_at_end_with_size(size_type n, const_reference value) {
        ASSERT_ARGUMENT_WITHIN_DOMAIN(n > 0, "The number of objects to construct n, must satisfy n > 0");
        ASSERT_ARGUMENT_WITHIN_DOMAIN(size() + n <= capacity(),
                                      "The container must have sufficient back spare capacity");
        RECORD_OLD_VALUE(old_size, size());
        CONTRACT_POSTCONDITION(size() == old_size + n, "The container size attribute must grow by n");

        // RAII wrapper that provides basic exception guarantee when
        // constructing value n times.
        ConstructionTransaction constructionTxn(this, end(), n);
        for (; constructionTxn.pos_ != constructionTxn.end_; ++constructionTxn.pos_) {
            construct_at_unchecked(constructionTxn.pos_, value);
        }
        set_size(size_ + n);
    }

    /**
     * @brief Copy constructs the range [first,last) at the end.
     *
     * @details This method accepts a pair of forward iterators. Hence, we know
     * the number of elements in the range, and we expect the caller to ensure
     * that there is enough room in the soa_split_buffer.
     * @tparam ForwardIterator
     * @param first The start of the range.
     * @param last One past the end of the range.
     */
    template <std::forward_iterator ForwardIterator>
    constexpr void construct_at_end_with_size(ForwardIterator first, ForwardIterator last) {
        ASSERT_ARGUMENT_WITHIN_DOMAIN(last > first, "The number of objects to construct n, must satisfy n > 0");
        ASSERT_ARGUMENT_WITHIN_DOMAIN(size() + std::distance(first, last),
                                      "The container must have sufficient back spare capacity");
        // RAII wrapper that provides basic exception guarantee when
        // constructing n elements.
        ConstructionTransaction txn(this, end(), std::distance(first, last));
        auto src_iter{first};
        for (; txn.pos_ != txn.end_; ++txn.pos_, ++src_iter) {
            construct_from_iterator_unchecked(txn.pos_, src_iter);
        }
        set_size(size_ + std::distance(first, last));
    }

    /**
     * @brief Moves the contents into a new larger allocation.
     *
     * @details Allocates a buffer with twice the current capacity (atleast 8) and relocates every element
     * into it. If the relocation throws mid-way, the new buffer destroys the elements it has received, and
     * *this is left unchanged.
     *
     */
    constexpr void grow_and_relocate(size_t new_front_spare) {
        ASSERT_ARGUMENT_WITHIN_DOMAIN(back_spare() == 0, "There should be no back spare capacity in the buffer");

        size_t old_capacity = capacity();
        size_t new_capacity = std::max({2 * old_capacity, 8uz});

        CONTRACT_POSTCONDITION(capacity() == new_capacity,
                               "The capacity() of the container should be updated new_capacity");

        split_buffer new_buf(new_capacity, new_front_spare);

        auto dest_iter = new_buf.begin();
        auto src_iter = begin();

        size_t new_buf_size{0};
        for (; src_iter != end(); ++src_iter, ++dest_iter) {
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
                construct_from_iterator_unchecked(dest_iter, std::move_iterator<iterator>(src_iter));
            } else {
                construct_from_iterator_unchecked(dest_iter, src_iter);
            }

            new_buf.set_sentinel(++new_buf_size);
        }
        // The new_buf destructor will cleanup after, when it goes out of scope and the old
        // data is destroyed.
        swap(*this, new_buf);
    }

    /**
     * @brief Copy constructs the elements from a data-stream, until a sentinel
     * value last is encountered.
     *
     * @details This method accepts an input_iterator and a sentinel. This is
     * common for example, when when reading from a file or network socket. So,
     * we do not know ahead of time how many elements need to be read. As a
     * result, in each iteration, we first check for sufficient back_spare()
     * capacity. If there is no room at the back, we perform a reallocation.
     * @tparam InputIterator An iterator that satisfies std::input_iterator
     * concept.
     * @tparam Sentinel
     * @param first
     * @param last
     */
    template <std::input_iterator InputIterator, typename Sentinel>
    constexpr void construct_at_end_with_sentinel(InputIterator first, Sentinel last) {

        while (first != last) {
            if (!back_spare()) {
                grow_and_relocate(0);
            }
            construct_at_unchecked(end(), *first);
            set_sentinel(size_ + 1);
            ++first;
        }
    }

    constexpr void shift_left_unchecked(size_t diff) {
        // Relocate [begin,end) to [begin-diff, end-diff)
        for (iterator it{begin()}; it != end(); ++it) {
            construct_from_iterator_unchecked(it - diff, std::move_iterator<iterator>(it));
            destruct_members_at(it);
        }
        set_valid_range(begin() - diff, end() - diff);
    }

    template <typename... MemberTypes>
    constexpr void emplace_front_members(MemberTypes&&... member_values) {
        if (front_spare() == 0) {
            if (back_spare() > 0) {
                if constexpr (std::is_nothrow_move_constructible_v<T>) {
                    size_t diff = back_spare();
                    diff = (diff + 1) / 2;
                    // shift_right_unchecked(diff);
                } else {
                    grow_and_relocate(capacity_ / 2);
                }
            } else {
                grow_and_relocate(capacity_ / 2);
            }
        }
        emplace_at_unchecked(begin() - 1, std::forward<MemberTypes>(member_values)...);
        set_valid_range(begin_offset_ - 1, size_ + 1);
    }

    /**
     * @brief Appends an element to the container.
     *
     * @tparam MemberTypes
     * @param member_values One value per member of T.
     */
    template <typename... MemberTypes>
    constexpr void emplace_back_members(MemberTypes&&... member_values) {
        if (back_spare() == 0) {
            if (front_spare() > 0) {
                if constexpr (std::is_nothrow_move_constructible_v<T>) {
                    size_t diff = front_spare();
                    diff = (diff + 1) / 2;
                    shift_left_unchecked(diff);
                } else {
                    grow_and_relocate(0);
                }
            } else {
                grow_and_relocate(0);
            }
        }
        emplace_at_unchecked(end(), std::forward<MemberTypes>(member_values)...);
        set_sentinel(size_ + 1);
    }

    template <typename... Args>
    consteval bool compare_arg_types_with_member_types() {
        constexpr bool isEqualLength = sizeof...(Args) == members.size();

        bool isIdenticalTypes{true};
        constexpr std::array<std::meta::info, members.size()> arg_types_reflection{^^std::remove_cvref_t<Args>...};

        for (auto i{0uz}; i < members.size(); ++i) {
            auto r1 = arg_types_reflection[i];
            auto r2 = type_of(members[i]);
            isIdenticalTypes &= std::meta::is_same_type(r1, r2);
        }
        return isEqualLength && isIdenticalTypes;
    }

    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    constexpr void emplace_front(Args&&... args) {
        if constexpr (compare_arg_types_with_member_types<Args...>()) {
            emplace_front_members(std::forward<Args>(args)...);
        } else {
            T value(std::forward<Args>(args)...);
            auto& [... mems] = value;
            emplace_front_members(mems...);
        }
    }

    /**
     * @brief Construct each of T element's members in place in the back spare capacity of the sub-arrays, one for each
     * member in contigous storage.
     *
     * @tparam Args
     * @param args
     * @return requires constexpr
     */
    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    constexpr void emplace_back(Args&&... args) {
        if constexpr (compare_arg_types_with_member_types<Args...>()) {
            emplace_back_members(std::forward<Args>(args)...);
        } else {
            T value(std::forward<Args>(args)...);
            auto& [... mems] = value;
            emplace_back_members(mems...);
        }
    }

    /**
     * @brief Swap the lhs with rhs, member-wise.
     *
     * @param lhs
     * @param rhs
     */
    friend void swap(split_buffer& lhs, split_buffer& rhs) noexcept {
        using std::swap;
        swap(lhs.raw_memory_, rhs.raw_memory_);
        swap(lhs.pointers_, rhs.pointers_);
        swap(lhs.begin_offset_, rhs.begin_offset_);
        swap(lhs.size_, rhs.size_);
        swap(lhs.capacity_, rhs.capacity_);
    }

    void copy_without_alloc(const split_buffer& other) noexcept {
        raw_memory_ = other.raw_memory_;
        pointers_ = other.pointers_;
        begin_offset_ = other.begin_offset_;
        size_ = other.size_;
        capacity_ = other.capacity_;
    }

    /**
     * @brief Destructs objects in the range [new_end, end).
     *
     * @param new_end
     */
    constexpr void destruct_from_end_to(iterator new_end) {
        ASSERT_ARGUMENT_WITHIN_DOMAIN(new_end >= begin() && new_end <= end(), "new_last must be in [begin(),end()]");
        CONTRACT_POSTCONDITION(size_ == new_end - begin(),
                               "The container size attribute must be updated to end() - begin()");
        CONTRACT_POSTCONDITION(new_end == end(), "The sentinel must be updated to new_end");
        auto iter = end();
        while (iter != new_end) {
            --iter;
            auto [... ptrs] = iter.ptrs_;
            (std::destroy_at(ptrs), ...);
        }
        size_ = (new_end - begin());
    }

    /**
     * @brief Destroys objects in the range [begin(),new_begin)
     *
     * @param new_begin
     */
    constexpr void destruct_from_begin_to(iterator new_begin) {
        ASSERT_ARGUMENT_WITHIN_DOMAIN(new_begin >= begin() && new_begin <= end(),
                                      "new_begin must be in [begin(), end()]");
        CONTRACT_POSTCONDITION(size_ == end() - new_begin,
                               "The container size attribute must be updated to end() - begin()");
        CONTRACT_POSTCONDITION(begin() == new_begin, "The container begin() must be updated to new_begin");

        iterator iter{begin()};
        while (iter != new_begin) {
            auto [... ptrs] = iter.ptrs_;
            (std::destroy_at(ptrs), ...);
            ++iter;
        }
        set_valid_range(iter, end());
    }

    constexpr void clear() { destruct_from_end_to(begin()); }

    /**
     * @brief Allocates uninitialized storage large enough to hold `num_elements` objects of type `T` laid out as one
     * sub-array per data member.
     *
     * @details num_elements counts `T` elements, not bytes. The byte size of the allocation is derived from it, by
     * `calculate_allocation_size<T>(num_elements)`: room for `num_elements` values for each member, plus any padding
     * needed to align each sub-array.
     *
     * The request is rounded up to a whole number of `StorageBlock`s and served by the allocator rebound to
     * `StorageBlock`. The returned address is therefore aligned to atleast `alignof(T)`.
     *
     * @param num_elements The user-requested capacity for the container in units of number of T elements.
     */
    constexpr std::byte* allocate_memory(size_t num_elements) {
        ASSERT_INTERNAL(begin() == nullptr, "split_buffer::allocate can only be called on an instance that hasn't "
                                            "allocated memory. This buffer either already "
                                            "owns memory, or a deallocation function didn't reset the layout");
        ASSERT_INTERNAL(
            size() == 0,
            "split_buffer::allocate can only be called on an instace that hasn't allocated memory and size() == 0.");
        ASSERT_INTERNAL(capacity() == 0, "split_buffer::allocate can only be called on an instance that hasn't "
                                         "allocated memory and capacity() == 0.");
        static constexpr size_t alignment = alignof(T);
        size_t num_bytes = calculate_allocation_size<T>(num_elements);

        // Rebind the allocator
        using traits = typename std::allocator_traits<Allocator>::template rebind_traits<StorageBlock>;
        using storage_block_allocator_type =
            typename std::allocator_traits<Allocator>::template rebind_alloc<StorageBlock>;

        storage_block_allocator_type storage_block_allocator(alloc_);
        size_t num_storage_blocks = (num_bytes + alignment - 1) / alignment;
        StorageBlock* storage_block_ptr = traits::allocate(storage_block_allocator, num_storage_blocks);
        return reinterpret_cast<std::byte*>(storage_block_ptr);
    }

    constexpr void deallocate_memory() {
        static constexpr size_t alignment = alignof(T);
        size_t nbytes = calculate_allocation_size<T>(capacity_);

        // Rebind the allocator
        using traits = typename std::allocator_traits<Allocator>::template rebind_traits<StorageBlock>;
        using storage_block_allocator_type =
            typename std::allocator_traits<Allocator>::template rebind_alloc<StorageBlock>;

        storage_block_allocator_type storage_block_allocator(alloc_);
        size_t num_storage_blocks = (nbytes + alignment - 1) / alignment;

        traits::deallocate(storage_block_allocator, raw_memory_, num_storage_blocks);
    }

    constexpr References operator[](size_t idx) { return *(begin() + idx); }

    /**
     * @brief Default constructor.
     */
    constexpr split_buffer() = default;

    /**
     * @brief Constructor that accepts a capacity and begin_offset.
     *
     */
    constexpr split_buffer(size_type capacity, size_type begin_offset) {
        ASSERT_ARGUMENT_WITHIN_DOMAIN(capacity > 0, "The capacity must be greater than 0");
        ASSERT_ARGUMENT_WITHIN_DOMAIN(begin_offset >= 0 && begin_offset < capacity,
                                      "Cannot have a start point outside the capacity");
        std::byte* raw_mem = allocate_memory(capacity);
        set_pointers(raw_mem, capacity);
        set_valid_range(begin_offset, 0);
    }

    /**
     * @brief Move constructor.
     *
     */
    constexpr split_buffer(split_buffer&& other) noexcept
        : raw_memory_{std::exchange(other.raw_memory_, nullptr)}
        , pointers_{std::move(other.pointers_)}
        , begin_offset_{std::exchange(other.begin_offset_, 0)}
        , size_{std::exchange(other.size_, 0)}
        , capacity_{std::exchange(other.capacity_, 0)} {
        other.reset();
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other
     * @return constexpr split_buffer&
     */
    constexpr split_buffer& operator=(split_buffer&& other) noexcept {
        split_buffer temp{std::move(other)};
        swap(*this, temp);
        return *this;
    }

    constexpr void reset() {
        raw_memory_ = nullptr;
        template for (constexpr auto ptr_member : ptr_members) { pointers_.[:ptr_member:] = nullptr; }
        set_begin_offset(0);
        set_size(0);
        set_capacity(0);
    }

    /**
     * @brief Destroy the split buffer object
     *
     */
    constexpr ~split_buffer() {
        clear();
        if (raw_memory_ != nullptr) {
            deallocate_memory();
            raw_memory_ = nullptr;
        }
    }

private:
    /**
     * @brief [Copy|Move] constructs the element *src_iter member-wise at dest_iter.
     *
     * @tparam SourceIterator
     * @param dest_iter
     * @param src_iter
     */
    template <typename SourceIterator>
    constexpr void construct_from_iterator_unchecked(iterator dest_iter, SourceIterator src_iter) {
        auto [... member_ptrs] = dest_iter.ptrs_;
        auto tup_ptrs = std::tuple(member_ptrs...);
        auto&& [... member_values] = *src_iter;
        if constexpr (std::is_same_v<is_move_iterator_t<SourceIterator>, std::true_type>) {
            construct_members_at(tup_ptrs, std::move(member_values)...);
        } else {
            construct_members_at(tup_ptrs, member_values...);
        }
    }

    /**
     * @brief This method is [copy|move] consructs T member-wise at the addresses specified per-member by the iterator
     * `dest_iter`.
     * @tparam ValueType
     * @param dest_iter
     * @param src_value
     */
    template <typename U>
    //    requires std::is_same_v<T, U>
    constexpr void construct_at_unchecked(iterator dest_iter, U&& src_value) {
        auto [... member_ptrs] = dest_iter.ptrs_;
        auto&& [... member_values] = src_value;
        construct_members_at(std::tuple(member_ptrs...), std::forward_like<U>(member_values)...);
    }

    /**
     * @brief This method is constructs T in place at the address specified by
     * `dest_iter` using constructor args.
     *
     * @tparam Args
     * @param dest_iter
     * @param args
     */
    template <typename... Args>
    constexpr void emplace_at_unchecked(iterator dest_iter, Args&&... args) {
        auto [... member_ptrs] = dest_iter.ptrs_;
        construct_members_at(std::tuple(member_ptrs...), std::forward<Args>(args)...);
    }
};
} // namespace soa::detail

#endif