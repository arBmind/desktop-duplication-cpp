#pragma once

#include "meta/opaque_value.h"

namespace win32 {

struct handle_tag;

struct handle_lifetime {
    using T = intptr_t;

    //static handle_lifetime copy_construct(void* self, const T& src);
    static handle_lifetime move_construct(void* self, T&& src);
    //template<class... Args>
    //static handle_lifetime construct(void* self, Args&&... args);
    static handle_lifetime construct(void* self);

    //void copy_assign(T& dst, const T& src);
    void move_assign(T& dst, T&& src);

    void destruct(const T& v) const;
};

struct handle_valuetrait {
    using type = intptr_t;
    static constexpr size_t size = sizeof(type);
    using lifetime = handle_lifetime;
};

} // namespace win32

namespace meta {

template<>
struct value_trait<win32::handle_tag> : win32::handle_valuetrait {};

} // namespace meta

namespace win32 {

using handle = meta::opaque_value<handle_tag>;

} // namespace win32
