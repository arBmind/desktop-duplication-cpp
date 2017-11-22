#pragma once

#include <vector>

template<class T>
struct array_view {

    explicit array_view(T *begin, size_t size)
        : begin_m(begin)
        , size_m(size) {}

    array_view(std::vector<T> &v)
        : array_view(v.data(), v.size()) {}

    template<size_t S>
    array_view(T (&a)[S])
        : array_view(a, S) {}

    static array_view from_bytes(uint8_t *data, size_t bytes) {
        return array_view(reinterpret_cast<T *>(data), bytes / sizeof(T));
    }
    static array_view from_bytes(const uint8_t *data, size_t bytes) {
        return array_view(reinterpret_cast<T *>(data), bytes / sizeof(T));
    }

    size_t size() const { return size_m; }
    bool empty() const { return size_m == 0; }

    T *begin() { return begin_m; }
    T *end() { return begin_m + size_m; }
    const T *begin() const { return begin_m; }
    const T *end() const { return begin_m + size_m; }

    T &operator[](size_t index) { return begin_m[index]; }
    const T &operator[](size_t index) const { return begin_m[index]; }

private:
    T *begin_m;
    size_t size_m;
};
