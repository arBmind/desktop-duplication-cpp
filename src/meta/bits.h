#pragma once
#include <initializer_list>
#include <inttypes.h>

namespace meta {

template<class T>
struct bits {
    static_assert(std::is_enum_v<T>, "flags only works for enums!");

    using enum_type = T;
    using value_type = std::underlying_type_t<T>;
    using bit_type = uint64_t;

    constexpr bits(const T &v) noexcept
        : bits(bit_type{1} << static_cast<value_type>(v)) {}
    constexpr bits(const bits &) noexcept = default;
    constexpr bits(bits &&) noexcept = default;

    constexpr bits &operator=(const bits &) noexcept = default;
    constexpr bits &operator=(bits &&) noexcept = default;

    template<class... Args>
    constexpr bits(const T &v, Args &&... args) noexcept
        : v(build(v, args...)) {}

    constexpr bool operator==(const bits &f) const noexcept { return v == f.v; }
    constexpr bool operator!=(const bits &f) const noexcept { return v != f.v; }

    constexpr bits operator|(const bits &f) const noexcept { return {v | f.v}; }

    constexpr bits clear(const bits &f) const noexcept { return {v & ~f.v}; }
    constexpr bool has_any(const bits &f) const noexcept { return (v & f.v) != 0; }
    constexpr bool has_all(const bits &f) const noexcept { return (v & f.v) == f.v; }

    template<class... Args>
    constexpr bool clear(const T &v, Args &&... args) const noexcept {
        return clear(build(v, args...));
    }
    template<class... Args>
    constexpr bool has_any(const T &v, Args &&... args) const noexcept {
        return has_any(build(v, args...));
    }
    template<class... Args>
    constexpr bool has_all(const T &v, Args &&... args) const noexcept {
        return has_all(build(v, args...));
    }

private:
    constexpr bits(const value_type &v) noexcept
        : v(v) {}

    template<class... Args>
    static constexpr value_type build(const Args &... args) noexcept {
        return (bits{0} | ... | bits{static_cast<const T &>(args)});
    }

private:
    value_type v;
};

} // namespace meta
