#pragma once

#include <array>

template <class A, class... B>
auto make_array(A&& a, B&&... b) {
	return std::array<std::decay_t<A>, 1 + sizeof...(B)>({ (A&&)a, (B&&)b... });
}
