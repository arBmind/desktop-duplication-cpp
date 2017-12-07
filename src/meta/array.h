#pragma once

#include <array>
#include <utility>

template<class A, class... B>
auto make_array(A &&a, B &&... b) noexcept {
    return std::array<std::decay_t<A>, (1 + sizeof...(B))>{
        {std::forward<A>(a), std::forward<B>(b)...}};
}
