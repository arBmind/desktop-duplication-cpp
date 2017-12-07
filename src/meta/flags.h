#pragma once
#include <initializer_list>
#include <type_traits>

namespace meta {

template<class T>
struct flags {
    static_assert(std::is_enum_v<T>, "flags only works for enums!");

    using enum_type = T;
    using value_type = std::underlying_type_t<T>;

    constexpr flags() noexcept = default;
    constexpr flags(const flags &) noexcept = default;
    constexpr flags(flags &&) noexcept = default;

    constexpr auto operator=(const flags &) noexcept -> flags & = default;
    constexpr auto operator=(flags &&) noexcept -> flags & = default;

    constexpr flags(T e) noexcept
        : v(static_cast<value_type>(e)) {}

    constexpr auto operator=(T e) noexcept -> flags {
        v = static_cast<value_type>(e);
        return *this;
    }

    constexpr void swap(flags &fl) noexcept { std::swap(v, fl.v); }

    template<class... Args>
    constexpr flags(T v, Args... args) noexcept
        : v(build(v, args...)) {}

    constexpr bool operator==(flags f) const noexcept { return v == f.v; }
    constexpr bool operator!=(flags f) const noexcept { return v != f.v; }

    constexpr auto operator|=(flags f2) noexcept -> flags & {
        v |= f2.v;
        return *this;
    }
    constexpr auto operator|=(T e2) noexcept -> flags & {
        v |= static_cast<value_type>(e2);
        return *this;
    }

    constexpr friend auto operator|(flags f1, flags f2) noexcept -> flags { return {f1.v | f2.v}; }

    constexpr auto clear(flags f = {}) const noexcept -> flags { return {v & ~f.v}; }
    constexpr bool has_any(flags f) const noexcept { return (v & f.v) != 0; }
    constexpr bool has_all(flags f) const noexcept { return (v & f.v) == f.v; }

    template<class... Args>
    constexpr bool clear(T v, Args... args) const noexcept {
        return clear(build(v, args...));
    }
    template<class... Args>
    constexpr bool has_any(T v, Args... args) const noexcept {
        return has_any(build(v, args...));
    }
    template<class... Args>
    constexpr bool has_all(T v, Args... args) const noexcept {
        return has_all(build(v, args...));
    }

private:
    constexpr flags(value_type v) noexcept
        : v(v) {}

    template<class... Args>
    static constexpr auto build(Args... args) noexcept -> flags {
        auto val = flags{0};
        const auto x = {((val |= args), 0)...};
        (void)x;
        return val;
    }

private:
    value_type v;
};

} // namespace meta

#define META_FLAGS_OP(T)                                                                           \
    constexpr auto operator|(T e1, T e2) noexcept->meta::flags<T> {                                \
        return meta::flags<T>(e1) | e2;                                                            \
    }
