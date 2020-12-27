#pragma once
#include <Windows.h>

#include <memory>

namespace win32 {

struct HandleCloser {
    void operator()(HANDLE handle) const { ::CloseHandle(handle); }
};
using Handle = std::unique_ptr<void, HandleCloser>;

} // namespace win32
