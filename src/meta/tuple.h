#pragma once

#include <memory>
#include <tuple>

template<class... A>
auto make_tuple_ptr(A &&... a) {
    return new std::tuple<std::decay_t<A>...>((A &&) a...);
}
template<class... A>
auto unique_tuple_ptr(ULONG_PTR p) {
    return std::unique_ptr<std::tuple<A...>>(reinterpret_cast<std::tuple<A...> *>(p));
}
