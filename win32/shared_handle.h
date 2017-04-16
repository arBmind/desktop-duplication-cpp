#pragma once

#include "meta/opaque_value.h"
#include "handle.h"

#include <atomic>

namespace win32 {

struct shared_handle_tag;
struct shared_handle_data;

struct shared_handle_lifetime {
    using T = shared_handle_data;

    static shared_handle_lifetime copy_construct(void* self, const T& src);
    static shared_handle_lifetime move_construct(void* self, T&& src);
    //template<class... Args>
    //static shared_handle_lifetime construct(void* self, Args&&... args);
    static shared_handle_lifetime construct(void* self);

    void copy_assign(T& dst, const T& src);
    void move_assign(T& dst, T&& src);

    void destruct(const T& v) const;
};

struct shared_handle_valuetrait {
    using type = shared_handle_data;
    static constexpr size_t size = sizeof(intptr_t) + sizeof(void*);
    using lifetime = shared_handle_lifetime;
};

} // namespace win32

namespace meta {

template<>
struct value_trait<win32::shared_handle_tag> : win32::shared_handle_valuetrait {};

} // namespace meta

namespace win32 {

using shared_handle = meta::opaque_value<shared_handle_tag>;

shared_handle to_shared(handle&&);

} // namespace win32
