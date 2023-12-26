#include "RenderThread.h"

namespace deskdup {

RenderThread::RenderThread(Config const &config)
    : m_windowRenderer{config} {}

RenderThread::~RenderThread() {
    if (m_stdThread) stop();
}

void RenderThread::start() {
    if (m_stdThread) return; // already started
    m_threadLoop.enableAwaitAlerts();
    m_stdThread.emplace([this] {
        m_thread = win32::Thread::fromCurrent();
        m_threadLoop.run();
    });
}

void RenderThread::stop() {
    m_thread.queueUserApc([this]() { m_threadLoop.quit(); });
    m_stdThread.reset();
}

void RenderThread::reset() {
    m_thread.queueUserApc([this]() {
        if (m_hasFrameHandle) {
            uninstallNextFrame();
        }
        m_windowRenderer.reset();
        m_hasFrameRendered = false;
        m_isDirty = false;
    });
}

void RenderThread::renderFrame() {
    if (m_hasFrameRendered) {
        m_isDirty = true;
        return;
    }
    m_windowRenderer.render();
    m_hasFrameRendered = true;
    m_isDirty = false;
    if (!m_hasFrameHandle) {
        installNextFrame();
    }
}

void RenderThread::updated() {
    if (!m_windowRenderer.isInitialized()) return;
    m_isDirty = true;
    if (!m_hasFrameHandle) {
        installNextFrame();
    }
}

void RenderThread::installNextFrame() {
    m_waitFrameHandle = m_windowRenderer.frameLatencyWaitable();
    m_threadLoop.addAwaitableMember<&RenderThread::nextFrame>(m_waitFrameHandle.get(), this);
    m_hasFrameHandle = true;
}

void RenderThread::uninstallNextFrame() {
    m_threadLoop.removeAwaitable(m_waitFrameHandle.get());
    m_waitFrameHandle.reset();
    m_hasFrameHandle = false;
}

auto RenderThread::nextFrame(HANDLE) -> win32::ThreadLoop::Keep {
    if (m_isDirty) {
        m_windowRenderer.render();
        m_isDirty = false;
        m_hasFrameRendered = true;
    }
    else if (m_hasFrameRendered) {
        m_hasFrameRendered = false;
    }
    else {
        uninstallNextFrame();
    }
    return win32::ThreadLoop::Keep::Yes;
}

} // namespace deskdup
