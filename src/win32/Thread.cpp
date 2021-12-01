#include "Thread.h"

#include <Windows.h>

namespace win32 {
namespace {

auto GetCurrentThreadHandle() -> HANDLE {
    auto output = HANDLE{};
    const auto process = GetCurrentProcess();
    const auto thread = GetCurrentThread();
    const auto desiredAccess = 0;
    const auto inheritHandle = false;
    const auto options = DUPLICATE_SAME_ACCESS;
    const auto success =
        DuplicateHandle(process, thread, process, &output, desiredAccess, inheritHandle, options);
    (void)success;
    return output;
}

} // namespace

auto Thread::fromCurrent() -> Thread { return Thread{GetCurrentThreadHandle()}; }

} // namespace win32
