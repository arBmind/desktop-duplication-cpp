#pragma once

template<class T>
struct scope_guard {
    using this_t = scope_guard;

    scope_guard(T &&f) noexcept
        : t((T &&) f) {}
    ~scope_guard() noexcept { t(); }

    scope_guard(const this_t &) = delete;
    auto operator=(const this_t &) -> this_t & = delete;
    scope_guard(this_t &&) = default;
    auto operator=(this_t &&) -> this_t & = default;

private:
    T t;
};

template<class T>
auto make_scope_guard(T &&f) noexcept {
    return scope_guard<T>((T &&) f);
}

#define SCOPE_GUARD_CAT(x, y) x##y
#define SCOPE_GUARD_ID(index) SCOPE_GUARD_CAT(__scope_guard, index)

#define LATER(expr) auto SCOPE_GUARD_ID(__LINE__){make_scope_guard([&]() noexcept { expr; })};
