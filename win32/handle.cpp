#include "handle.h"

#include "intern/wrapper.h"

namespace win32 {

handle_lifetime handle_lifetime::move_construct(void *self, T &&src) {
    *reinterpret_cast<T*>(self) = src;
    src = -1;
    return {};
}

handle_lifetime handle_lifetime::construct(void *self) {
    *reinterpret_cast<T*>(self) = -1;
    return {};
}

void handle_lifetime::move_assign(T &dst, T &&src) {
    dst = src;
}

void handle_lifetime::destruct(const T &v) const {
    intern::CloseHandle(v);
}

} // namespace win32
