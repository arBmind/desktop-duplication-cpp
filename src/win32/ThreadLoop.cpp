#include "ThreadLoop.h"

namespace win32 {

void ThreadLoop::sleep() {
    auto handleCount = static_cast<DWORD>(m_waitHandles.size());
    auto handleData = m_waitHandles.data();
    auto wakeMask = m_queueStatus;
    auto flags = (m_awaitAlerts ? MWMO_ALERTABLE : 0u) | (m_queueStatus != 0 ? MWMO_INPUTAVAILABLE : 0u);

    auto timeout = static_cast<uint32_t>(m_timeout.count());
    auto awoken = ::MsgWaitForMultipleObjectsEx(handleCount, handleData, timeout, wakeMask, flags);
    if (awoken >= WAIT_OBJECT_0 && awoken < WAIT_OBJECT_0 + handleCount) {
        auto i = awoken - WAIT_OBJECT_0;
        auto keep = m_callbacks[i](m_waitHandles[i]);
        if (keep == Keep::Remove) {
            m_callbacks.erase(m_callbacks.begin() + i);
            m_waitHandles.erase(m_waitHandles.begin() + i);
        }
    }
    else if (awoken == WAIT_OBJECT_0 + handleCount) {
        processCurrentMessages();
    }
    else if (awoken == WAIT_IO_COMPLETION) {
        // APC - already called
    }
    else {
        OutputDebugStringA("Unhandled Awoken\n");
    }
}

void ThreadLoop::processCurrentMessages() {
    while (true) {
        auto msg = MSG{};
        const auto success = ::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
        if (0 == success) {
            return;
        }
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (WM_QUIT == msg.message) {
            m_quit = true;
            m_returnValue = reinterpret_cast<int &>(msg.wParam);
        }
    }
}

} // namespace win32
