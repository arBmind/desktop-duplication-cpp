#pragma once
#include "CaptureThread.h"
#include "FrameUpdater.h"
#include "MainController.h"
#include "Model.h"
#include "PointerUpdater.h"
#include "RenderThread.h"

#include "win32/Thread.h"
#include "win32/ThreadLoop.h"
#include "win32/WaitableTimer.h"
#include "win32/Window.h"

#include <optional>

namespace deskdup {

using win32::Handle;
using win32::Thread;
using win32::ThreadLoop;
using win32::WaitableTimer;
using win32::Window;
using win32::WindowClass;
using win32::WindowWithMessages;

/// main controller that manages the duplication party
/// notes:
/// * all public methods are called from the MainThread
/// * capturing happens in the CaptureThread
/// * rendering happens in the RenderThread
struct DuplicationController {
    struct Args {
        WindowClass const &windowClass;
        MainController &mainController;
        Thread &mainThread;
        WindowWithMessages &outputWindow;
    };
    DuplicationController(Args const &);
    ~DuplicationController();

    void updateDuplicationStatus(DuplicationStatus);
    void updateOutputDimension(Dimension);
    void updateOutputZoom(float zoom);
    void updateCaptureOffset(Vec2f);
    void restart();

private:
    enum class Status {
        Stopped, // from Stopping
        Initial = Stopped,
        Starting, // from Initial/Stopped
        Live, // from Starting or Resuming
        Stopping, // from Live
        Paused, // from Live
        Resuming, // from Paused
        RetryStarting, // from Starting
        Failed, // given up
    };

private:
    auto renderThreadConfig(OperationModeLens) -> RenderThread::Config;
    auto captureThreadConfig() -> CaptureThread::Config;

    void startOnMain();
    void pauseOnMain();
    void stopOnMain();
    void resetOnMain();
    void updateStatusOnMain(Status);

    void initCaptureThread();
    void updateCaptureStatus();
    void startCaptureThread(HANDLE targetHandle);
    void awaitRetry();
    void retryTimeout();

private:
    auto forwardRenderMessage(const win32::WindowMessage &) -> win32::OptLRESULT;

private:
    friend struct ::WindowRenderer;
    friend struct ::CaptureThread;
    void setError(const std::exception_ptr &); // called on CaptureThread or RenderThread
    void setFrame(CapturedUpdate &&, const FrameContext &, size_t threadIndex); // called on RenderThread

private:
    void setFrameOnRender(CapturedUpdate &&, const FrameContext &, size_t threadIndex);

private:
    std::atomic<Status> m_status{};
    Rect m_displayRect{};

    WindowClass const &m_windowClass;
    MainController &m_controller;
    Thread &m_mainThread;
    WindowWithMessages &m_outputWindow;
    WindowWithMessages m_renderWindow;

    RenderThread m_renderThread;
    CaptureThread m_captureThread;

    ComPtr<ID3D11Texture2D> m_targetTexture;
    std::optional<FrameUpdater> m_frameUpdater;
    PointerUpdater m_pointerUpdater;

    WaitableTimer m_retryTimer;
};

} // namespace deskdup
