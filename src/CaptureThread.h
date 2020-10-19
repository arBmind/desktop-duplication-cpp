#pragma once
#include "stable.h"

#include "FrameContext.h"

#include <meta/comptr.h>

#include <optional>
#include <thread>
#include <vector>

struct CapturedUpdate;

struct Unexpected {
    const char *text{};
};

struct Expected {
    const char *text{};
};

/// returns the global thread handle (usable in any thread!)
HANDLE GetCurrentThreadHandle();

struct CaptureThread {
    struct Callbacks { // not implemented here! (see application.cpp)
        void setError(std::exception_ptr Error);
        void setFrame(CapturedUpdate &&frame, const FrameContext &context, size_t thread_index);
    };
    struct InitArgs {
        Callbacks *callbacks{}; // callbacks
        int display{}; // index of the display to capture
        size_t threadIndex{}; // identifier to this thread
    };
    struct StartArgs {
        ComPtr<ID3D11Device> device; // device used for capturing
        POINT offset{}; // offset from desktop to target coordinates
    };

    CaptureThread(InitArgs &&args) noexcept
        : m_callbacks(args.callbacks)
        , m_display(args.display)
        , m_index(args.threadIndex) {}

    [[nodiscard]] auto start(StartArgs &&args) -> std::thread; // start a stopped thread

    void next(); // thread starts to capture the next frame
    void stop(); // signal thread to stop, use join to wait!

private:
    static void WINAPI stopAPC(ULONG_PTR);
    static void WINAPI nextAPC(ULONG_PTR);

    void run();

    void initDuplication();
    void
    handleDeviceError(const char *text, HRESULT result, std::initializer_list<HRESULT> expected);

    auto captureUpdate() -> std::optional<CapturedUpdate>;

private:
    Callbacks *m_callbacks;
    int m_display;
    size_t m_index;
    FrameContext m_context{};
    ComPtr<ID3D11Device> m_device;
    HANDLE m_threadHandle{};
    bool m_keepRunning = true;
    bool m_doCapture = true;

    ComPtr<IDXGIOutputDuplication> m_dupl;
};
