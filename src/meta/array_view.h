#pragma once

#include <vector>

template<class T>
struct array_view {

    constexpr explicit array_view(T *begin, size_t size) noexcept
        : begin_m(begin)
        , size_m(size) {}

    array_view(std::vector<T> &v) noexcept
        : array_view(v.data(), v.size()) {}

    template<size_t S>
    constexpr array_view(T (&a)[S]) noexcept
        : array_view(a, S) {}

    static array_view from_bytes(uint8_t *data, size_t bytes) noexcept {
        return array_view(reinterpret_cast<T *>(data), bytes / sizeof(T));
    }
    static array_view from_bytes(const uint8_t *data, size_t bytes) noexcept {
        [[gsl::suppress("26490")]] // required by definition
        return array_view(reinterpret_cast<T *>(data), bytes / sizeof(T));
    }

    constexpr size_t size() const noexcept { return size_m; }
    constexpr bool empty() const noexcept { return size_m == 0; }

    constexpr T *begin() noexcept { return begin_m; }
    T *end() noexcept { return begin_m + size_m; }
    constexpr const T *begin() const noexcept { return begin_m; }
    const T *end() const noexcept { return begin_m + size_m; }

    constexpr T &operator[](size_t index) noexcept { return begin_m[index]; }
    constexpr const T &operator[](size_t index) const noexcept { return begin_m[index]; }

private:
    T *begin_m;
    size_t size_m;
};
