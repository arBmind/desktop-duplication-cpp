#pragma once

#include <variant>

template<class Value, class Error = int>
struct result {
    template<class T>
    result(T &&v)
        : store_m(v) {}

    bool is_error() const { return store_m.index() == 1; }
    bool is_value() const { return store_m.index() == 0; }

    const Value &value() const { return std::get<0>(store_m); }
    const Value &cvalue() const { return std::get<0>(store_m); }
    Value &value() { return std::get<0>(store_m); }

    const Value &error() const { return std::get<1>(store_m); }
    const Value &cerror() const { return std::get<1>(store_m); }
    Value &error() { return std::get<1>(store_m); }

    operator bool() const { return is_value(); }
    const Value &operator*() const { return value(); }
    Value &operator*() { return value(); }

private:
    std::variant<Value, Error> store_m;
};
