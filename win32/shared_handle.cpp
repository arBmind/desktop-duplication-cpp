#include "shared_handle.h"

#include "intern/wrapper.h"

namespace {

struct shared_counter {
    void ref() {
        m_use_count.fetch_add(1);
    }
    void unref() {
        m_use_count.fetch_sub(1);
    }
    bool is_zero() {
        return m_use_count.load() == 0;
    }

private:
    std::atomic_uint_fast32_t m_use_count { 1 };
};

}

namespace win32 {

struct shared_handle_data {
    intptr_t handle;
    shared_counter* counter;
};

shared_handle_lifetime shared_handle_lifetime::copy_construct(void *self, const T &src) {
    *reinterpret_cast<T*>(self) = src;
    if (src.counter) src.counter->ref();
    return {};
}

shared_handle_lifetime shared_handle_lifetime::move_construct(void *self, T &&src) {
    *reinterpret_cast<T*>(self) = src;
    src.counter = nullptr;
    return {};
}

shared_handle_lifetime shared_handle_lifetime::construct(void *self) {
    new (self) T{ -1, nullptr };
    return {};
}

void shared_handle_lifetime::copy_assign(T &dst, const T &src) {
    destruct(dst);
    dst = src;
    if (src.counter) src.counter->ref();
}

void shared_handle_lifetime::move_assign(T &dst, T &&src) {
    destruct(dst);
    dst = src;
    src.counter = nullptr;
}

void shared_handle_lifetime::destruct(const T &v) const {
    if (nullptr == v.counter) return;
    v.counter->unref();
    if (v.counter->is_zero()) {
        intern::CloseHandle(v.handle);
        delete v.counter;
    }
}

shared_handle to_shared(handle &&h) {
    auto result = shared_handle{};
    auto& t = result.unwrap();
    t.counter = new shared_counter{};
    t.handle = h.unwrap();
    h.unwrap() = -1;
    return result;
}

} // namespace win32
