#pragma once
#include <type_traits>
#include <initializer_list>

namespace meta {

template<class T>
struct flags {
    using value_type = std::underlying_type_t<T>;

    constexpr flags(const T& v) noexcept : flags(static_cast<value_type>(v)) {}
    constexpr flags(const flags&) noexcept = default;
    constexpr flags(flags&&) noexcept = default;

    constexpr flags& operator=(const flags&) noexcept = default;
    constexpr flags& operator=(flags&&) noexcept = default;

    template<class... Args>
    constexpr flags(T&& v, Args&&... args) noexcept : v(build(v, args...)) {}

    constexpr bool operator==(const flags& f) const noexcept { return v == f.v; }
    constexpr bool operator!=(const flags& f) const noexcept { return v != f.v; }

    constexpr flags operator|(const flags& f) const noexcept { return {v | f.v}; }

    constexpr flags clear(const flags& f) const noexcept { return {v & ~f.v}; }
    constexpr bool has_any(const flags& f) const noexcept { return (v & f.v) != 0; }
    constexpr bool has_all(const flags& f) const noexcept { return (v & f.v) == f.v; }

    template<class... Args>
    constexpr bool clear(T&& v, Args&&... args) const noexcept {
        return clear(build(v, args...));
    }
    template<class... Args>
    constexpr bool has_any(T&& v, Args&&... args) const noexcept {
        return has_any(build(v, args...));
    }
    template<class... Args>
    constexpr bool has_all(T&& v, Args&&... args) const noexcept {
        return has_all(build(v, args...));
    }
private:
    constexpr flags(const value_type& v) noexcept : v(v) {}

    template<class... Args>
    static constexpr value_type build(const Args&... args) noexcept {
        auto val = flags{0};
        auto x = {((val = val | flags{static_cast<const T&>(args)}), 0) ...};
        (void)x;
        return val.v;
    }

private:
    value_type v;
};

} // namespace meta
