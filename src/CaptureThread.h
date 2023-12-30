#pragma once
#include "FrameContext.h"

#include "meta/comptr.h"
#include "win32/Geometry.h"
#include "win32/Thread.h"

#include <Windows.h>
#include <d3d11.h>

#include <optional>
#include <thread>

struct CapturedUpdate;

struct Unexpected {
    const char *text{};
};

struct Expected {
    const char *text{};
};

/// returns the global thread handle (usable in any thread!)
HANDLE GetCurrentThreadHandle();

using win32::Point;
using win32::Thread;

/// Interaction with the Capturing Thread
/// note: all public methods should be called by main thread!
struct CaptureThread {
    using SetErrorFunc = void(void *, const std::exception_ptr &);
    using SetFrameFunc = void(void *, CapturedUpdate &&, const FrameContext &, size_t threadIndex);

    struct Config {
        size_t threadIndex{}; // identifier to this thread

        SetErrorFunc *setErrorCallback{&CaptureThread::noopSetErrorCallback};
        SetFrameFunc *setFrameCallback{&CaptureThread::noopSetFrameCallback};
        void *callbackPtr{};

        /// note: callbacks are invoked inside the CaptureThread!
        template<class T>
        void setCallbacks(T *p) {
            callbackPtr = p;
            setErrorCallback = [](void *ptr, const std::exception_ptr &exception) {
                auto *cb = std::bit_cast<T *>(ptr);
                cb->setError(exception);
            };
            setFrameCallback = [](void *ptr, CapturedUpdate &&update, const FrameContext &context, size_t threadIndex) {
                auto *cb = std::bit_cast<T *>(ptr);
                cb->setFrame(std::move(update), context, threadIndex);
            };
        }
    };

    CaptureThread(const Config &config) noexcept
        : m_config(config) {}

    ~CaptureThread();

    struct StartArgs {
        int display{}; // index of the display to capture
        ComPtr<ID3D11Device> device; // device used for capturing
        Point offset{}; // offset from desktop to target coordinates
    };
    void start(StartArgs &&args); ///< start a stopped thread

    void next(); ///< thread starts to capture the next frame
    void stop(); ///< signal thread to stop and waits

private:
    void capture_next();
    void capture_stop();

    void run();

    void initCapture();
    void handleDeviceError(const char *text, HRESULT, std::initializer_list<HRESULT> expected);

    auto captureUpdate() -> std::optional<CapturedUpdate>;

    static void noopSetErrorCallback(void *, const std::exception_ptr &) {}
    static void noopSetFrameCallback(void *, CapturedUpdate &&, const FrameContext &, size_t /*threadIndex*/) {}

private:
    Config m_config;
    int m_display{};

    FrameContext m_context{};
    ComPtr<ID3D11Device> m_device{};
    Thread m_thread{};
    bool m_keepRunning = true;

    bool m_doCapture = true;

    ComPtr<IDXGIOutputDuplication> m_dupl;
    std::optional<std::jthread> m_stdThread;
};
