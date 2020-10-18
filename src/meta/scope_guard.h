#pragma once

template<class T>
struct scope_guard {
    scope_guard(T &&f) noexcept
        : t((T &&) f) {}
    ~scope_guard() noexcept { t(); }

    scope_guard(const scope_guard &) = delete;
    auto operator=(const scope_guard &) -> scope_guard & = delete;
    scope_guard(scope_guard &&) = default;
    auto operator=(scope_guard &&) -> scope_guard & = default;

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
