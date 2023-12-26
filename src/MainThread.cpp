#include "MainThread.h"

namespace deskdup {

MainThread::MainThread(win32::WindowClass::Config windowClassConfig)
    : m_windowClass{windowClassConfig}
    , m_thread{win32::Thread::fromCurrent()} {

    m_threadLoop.enableAwaitAlerts();
}

int MainThread::run() { return m_threadLoop.run(); }

} // namespace deskdup
