#pragma once

#include <optional>

namespace meta {

template<class T>
using optional = std::optional<T>;

// template <class T>
// struct optional {
//
//
// private:
//	std::variant<std::monostate, T> data;
//};

} // namespace meta
