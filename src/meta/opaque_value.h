#pragma once

#include <memory>
#include <type_traits>

namespace meta {

template<class T>
struct default_lifetime {
    static auto copy_construct(void *self, const T &src) {
        new (self) T(src);
        return default_lifetime{};
    }
    static auto move_construct(void *self, T &&src) {
        new (self) T((T &&) src);
        return default_lifetime{};
    }
    template<class... Args>
    static auto construct(void *self, Args &&... args) {
        new (self) T((Args &&) args...);
        return default_lifetime{};
    }

    void copy_assign(T &dst, const T &src) { dst = src; }
    void move_assign(T &dst, T &&src) { dst = (T &&) src; }

    void destruct(const T &v) const { v.~T(); }
};

template<class T>
struct lifetime : default_lifetime<T> {};

template<class T>
struct default_value_trait {
    using type = T;
    static constexpr size_t size = sizeof(type);
    using lifetime = default_lifetime<type>;
};

// specialize this trait for your use case
template<class Tag>
struct value_trait : default_value_trait<Tag> {};

// generic opaque value wrapper
template<class Tag>
struct opaque_value {
    using value_trait = value_trait<Tag>;

    using type = typename value_trait::type;
    static constexpr const size_t size = value_trait::size;
    using lifetime = typename value_trait::lifetime;

    opaque_value(const opaque_value &src)
        : m_lifetime(lifetime::copy_construct(this, src.unwrap())) {}

    opaque_value(opaque_value &&src)
        : m_lifetime(lifetime::move_construct(this, (type &&) src.unwrap())) {}

    template<class... Args>
    opaque_value(Args &&... args)
        : m_lifetime(lifetime::construct(this, (Args &&) args...)) {}

    ~opaque_value() { m_lifetime.destruct(unwrap()); }

    opaque_value &operator=(const opaque_value &src) {
        m_lifetime.copy_assign(unwrap(), src.unwrap());
        return *this;
    }
    opaque_value &operator=(opaque_value &&src) {
        m_lifetime.move_assign(unwrap(), (type &&) src.unwrap());
        return *this;
    }

    // you are doing it wrong! - if you use outside of the implementation
    type &unwrap() {
        static_assert(sizeof(type) == size, "wrong type?");
        return reinterpret_cast<type &>(m_memory);
    }

    const type &unwrap() const {
        static_assert(sizeof(type) == size, "wrong type?");
        return reinterpret_cast<const type &>(m_memory);
    }

private:
    uint8_t m_memory[size];
    lifetime m_lifetime;
};

} // namespace meta
