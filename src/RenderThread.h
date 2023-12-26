#pragma once
#include "WindowRenderer.h"
#include "win32/Thread.h"
#include "win32/ThreadLoop.h"

#include <optional>
#include <thread>

namespace deskdup {

struct RenderThread {
    using Config = WindowRenderer::Args;
    explicit RenderThread(Config const &);
    ~RenderThread();

    auto thread() -> win32::Thread & { return m_thread; }
    auto threadLoop() -> win32::ThreadLoop & { return m_threadLoop; }
    auto windowRenderer() -> WindowRenderer & { return m_windowRenderer; }

    void start();
    void stop();

    void reset();
    void renderFrame();
    void updated();

private:
    void installNextFrame();
    void uninstallNextFrame();

    auto nextFrame(HANDLE) -> win32::ThreadLoop::Keep;

private:
    win32::Thread m_thread{};
    win32::ThreadLoop m_threadLoop{};

    WindowRenderer m_windowRenderer;

    bool m_hasFrameRendered{};
    bool m_isDirty{};
    bool m_hasFrameHandle{};
    Handle m_waitFrameHandle{};

    std::optional<std::jthread> m_stdThread;
};

} // namespace deskdup
