#pragma once
#include <bit>
#include <span>

template<class S>
auto fromByteSpan(std::span<const uint8_t> &src) -> S {
    using T = typename S::element_type;
    static_assert(alignof(T) <= sizeof(T));
    return S{std::bit_cast<T *>(src.data()), src.size() / sizeof(T)};
}
