#pragma once

template<class T>
struct scope_guard {
    scope_guard(T&& f) : t((T&&)f) {}
    ~scope_guard() { t(); }

private:
    T t;
};

template<class T>
auto make_scope_guard(T&& f) {
    return scope_guard<T>((T&&)f);
}

#define SCOPE_GUARD_CAT(x,y)    x##y
#define SCOPE_GUARD_ID(index)   SCOPE_GUARD_CAT(__scope_guard, index)

#define LATER(expr) auto SCOPE_GUARD_ID(__LINE__) { make_scope_guard([&]{ expr; }) };
