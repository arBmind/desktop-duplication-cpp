#pragma once
#include "CaptureThread.h"
#include "FrameUpdater.h"
#include "Model.h"
#include "PointerUpdater.h"
#include "WindowRenderer.h"

#include "win32/Thread.h"
#include "win32/ThreadLoop.h"
#include "win32/WaitableTimer.h"
#include "win32/Window.h"

#include <memory> // std::unique_ptr
#include <optional>

namespace deskdup {

using win32::Handle;
using win32::Optional;
using win32::Thread;
using win32::ThreadLoop;
using win32::WaitableTimer;
using win32::Window;

using CaptureThreadPtr = std::unique_ptr<CaptureThread>;

/// main controller that manages the duplication party
struct DuplicationController {
    DuplicationController(Model &, ThreadLoop &, Window &);

    void update();

private:
    void initCaptureThread();
    void updateCaptureStatus();
    void start();
    void startCaptureThread(HANDLE targetHandle);
    void stop();
    void awaitRetry();

    void render();
    void captureNextFrame();

    void awaitNextFrame();
    void stopNextFrame();
    auto nextFrame(HANDLE) -> ThreadLoop::Keep;

    friend struct ::CaptureThread;
    void setError(std::exception_ptr);
    void main_setError(std::exception_ptr);
    void rethrow();

    void setFrame(CapturedUpdate &&, const FrameContext &, size_t threadIndex);
    void main_setFrame(CapturedUpdate &&, const FrameContext &, size_t threadIndex);

    void retryTimeout();

private:
    CaptureModel &m_captureModel;
    DuplicationModel &m_duplicationModel;
    ThreadLoop &m_threadLoop;
    Window &m_outputWindow;

    Thread m_mainThread;
    Optional<CaptureThread> m_captureThread;
    bool m_triggerCaptureThread{};

    CaptureStatus m_lastCaptureStatus{};
    bool m_isFreezed{};
    ComPtr<ID3D11Texture2D> m_targetTexture;
    WindowRenderer m_windowRenderer;

    Optional<FrameUpdater> m_frameUpdater;
    PointerUpdater m_pointerUpdater;
    bool m_hasFrameUpdated{};
    bool m_hasLastUpdate{};

    bool m_waitFrame = false;
    Handle m_waitFrameHandle{};

    bool m_hasError = false;
    std::exception_ptr m_error;

    WaitableTimer m_retryTimer;
};

} // namespace deskdup
