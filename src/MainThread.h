#pragma once
#include "win32/Thread.h"
#include "win32/ThreadLoop.h"
#include "win32/Window.h"

namespace deskdup {

struct MainThread final {
    explicit MainThread(win32::WindowClass::Config windowClassConfig);

    // note: only use the windowClass from the main thread!
    auto windowClass() -> win32::WindowClass & { return m_windowClass; }

    auto thread() -> win32::Thread & { return m_thread; }
    auto threadLoop() -> win32::ThreadLoop & { return m_threadLoop; }

    int run();

private:
    win32::WindowClass m_windowClass;
    win32::Thread m_thread{};
    win32::ThreadLoop m_threadLoop{};
};

} // namespace deskdup
