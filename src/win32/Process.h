#pragma once
#include "Dpi.h"

#include <Windows.h>

namespace win32 {

/// Wrapper for a win32 Process
struct Process {
    /// set default DpiAwareness for current process
    /// note: WindowClasses and Threads use it (set it before creating them)
    static void current_setDpiAwareness(DpiAwareness);

private:
    // HANDLE m_processHandle{};
};

} // namespace win32
