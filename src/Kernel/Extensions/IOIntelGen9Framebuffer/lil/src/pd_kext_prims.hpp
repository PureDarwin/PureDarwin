#pragma once
/*
 * pd_kext_prims.hpp - PureDarwin de-STL primitives for building `lil` inside a
 * Darwin kext (freestanding libkern C++, no libc++/STL). Replaces the compile-
 * time and lightweight container std:: facilities lil's gen9 path relies on with
 * header-only equivalents (clang builtins + tiny types). No exceptions, no RTTI,
 * no runtime. Everything lives in namespace `lil` (NOT std); call sites are
 * rewritten std::X -> lil::X as the STL is stripped. See pd-intel-gen9 notes.
 */
#include <stddef.h>
#include <stdint.h>

namespace lil {

// type traits (no <type_traits>)
template <class A, class B> inline constexpr bool same_as = __is_same(A, B);
template <class T> inline constexpr bool is_trivial_v = __is_trivial(T);
template <class T> inline constexpr bool is_standard_layout_v = __is_standard_layout(T);
template <class D, class B> concept derived_from = __is_base_of(B, D);

template <class T> struct remove_ref { using type = T; };
template <class T> struct remove_ref<T &> { using type = T; };
template <class T> struct remove_ref<T &&> { using type = T; };
template <class T> struct remove_cv { using type = T; };
template <class T> struct remove_cv<const T> { using type = T; };
template <class T> struct remove_cv<volatile T> { using type = T; };
template <class T> struct remove_cv<const volatile T> { using type = T; };
template <class T> using remove_cvref_t = typename remove_cv<typename remove_ref<T>::type>::type;

template <class T, T v> struct integral_constant { static constexpr T value = v; };

// std::to_underlying replacement.
template <class E>
constexpr __underlying_type(E) to_underlying(E e) {
    return static_cast<__underlying_type(E)>(e);
}

// iterator plumbing
struct forward_iterator_tag {};

template <class T, size_t N> constexpr T *begin(T (&a)[N]) { return a; }
template <class T, size_t N> constexpr T *end(T (&a)[N]) { return a + N; }
template <class C> constexpr auto begin(C &c) -> decltype(c.begin()) { return c.begin(); }
template <class C> constexpr auto end(C &c) -> decltype(c.end()) { return c.end(); }
template <class C> constexpr auto begin(const C &c) -> decltype(c.begin()) { return c.begin(); }
template <class C> constexpr auto end(const C &c) -> decltype(c.end()) { return c.end(); }

// array (aggregate + CTAD guide)
template <class T, size_t N>
struct array {
    T _d[N];
    constexpr T *data() { return _d; }
    constexpr const T *data() const { return _d; }
    constexpr size_t size() const { return N; }
    constexpr T &operator[](size_t i) { return _d[i]; }
    constexpr const T &operator[](size_t i) const { return _d[i]; }
    constexpr T *begin() { return _d; }
    constexpr T *end() { return _d + N; }
    constexpr const T *begin() const { return _d; }
    constexpr const T *end() const { return _d + N; }
};
template <class T, class... U> array(T, U...) -> array<T, 1 + sizeof...(U)>;

// pair
template <class A, class B>
struct pair {
    A first{};
    B second{};
};

// span
template <class T>
struct span {
    T *ptr = nullptr;
    size_t len = 0;

    constexpr span() = default;
    constexpr span(T *p, size_t n) : ptr(p), len(n) {}
    template <size_t N> constexpr span(T (&arr)[N]) : ptr(arr), len(N) {}

    constexpr T *data() const { return ptr; }
    constexpr size_t size() const { return len; }
    constexpr bool empty() const { return len == 0; }
    constexpr T &operator[](size_t i) const { return ptr[i]; }
    constexpr T *begin() const { return ptr; }
    constexpr T *end() const { return ptr + len; }
};

// optional
struct nullopt_t {};
inline constexpr nullopt_t nullopt{};

template <class T>
struct optional {
    T value_{};
    bool has_ = false;

    constexpr optional() = default;
    constexpr optional(nullopt_t) {}
    constexpr optional(const T &v) : value_(v), has_(true) {}

    constexpr bool has_value() const { return has_; }
    constexpr explicit operator bool() const { return has_; }
    constexpr T &value() { return value_; }
    constexpr const T &value() const { return value_; }
    constexpr T value_or(T fb) const { return has_ ? value_ : fb; }
    constexpr T &operator*() { return value_; }
    constexpr const T &operator*() const { return value_; }
    constexpr T *operator->() { return &value_; }
};

// expected / unexpected (no exceptions)
template <class E>
struct unexpected_t {
    E err;
};
template <class E>
constexpr unexpected_t<E> unexpected(E e) { return unexpected_t<E>{e}; }

template <class T, class E>
struct expected {
    T value_{};
    E error_{};
    bool has_ = false;

    constexpr expected() : has_(true) {}
    constexpr expected(const T &v) : value_(v), has_(true) {}
    constexpr expected(unexpected_t<E> u) : error_(u.err), has_(false) {}

    constexpr bool has_value() const { return has_; }
    constexpr explicit operator bool() const { return has_; }
    constexpr T &value() { return value_; }
    constexpr const T &value() const { return value_; }
    constexpr E error() const { return error_; }
    constexpr T &operator*() { return value_; }
    constexpr const T &operator*() const { return value_; }
    constexpr T *operator->() { return &value_; }
};

// string_view (minimal, byte comparison)
struct string_view {
    const char *ptr = nullptr;
    size_t len = 0;

    constexpr string_view() = default;
    constexpr string_view(const char *p, size_t n) : ptr(p), len(n) {}

    constexpr bool operator==(const string_view &o) const {
        if (len != o.len)
            return false;
        for (size_t i = 0; i < len; i++)
            if (ptr[i] != o.ptr[i])
                return false;
        return true;
    }
    constexpr bool operator!=(const string_view &o) const { return !(*this == o); }
};

// ranges algorithms (whole-range overloads)
namespace ranges {

template <class R, class P>
constexpr bool any_of(R &&r, P pred) {
    for (auto &&e : r)
        if (pred(e))
            return true;
    return false;
}

template <class R, class P>
constexpr auto find_if(R &&r, P pred) -> decltype(lil::begin(r)) {
    auto b = lil::begin(r);
    auto e = lil::end(r);
    for (; b != e; ++b)
        if (pred(*b))
            return b;
    return e;
}

} // namespace ranges

} // namespace lil
