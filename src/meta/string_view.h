#pragma once
#include <cstring>
#include <string>

namespace meta {

template<class Char>
struct string_view {

    constexpr string_view()
        : m_ptr(nullptr)
        , m_count(0) {}
    constexpr string_view(const string_view &) noexcept = default;
    string_view(string_view &&) noexcept = default;

    string_view &operator=(const string_view &) noexcept = default;
    string_view &operator=(string_view &&) noexcept = default;

    bool operator==(const string_view &o) {
        return o.count() == count() && memcmp(m_ptr, o.m_ptr, m_count * sizeof(Char)) == 0;
    }
    bool operator!=(const string_view &o) { return !(*this == o); }

    Char *data() const { return m_ptr; }
    size_t count() const { return m_count; }

    string_view substr(size_t index, size_t count) {
        if (index >= m_count) return {};
        if (count > m_count - index) count = m_count - index;
        return {m_ptr + index, count};
    }

private:
    Char *m_ptr;
    size_t m_count;
};

} // namespace meta
