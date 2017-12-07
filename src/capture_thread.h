#pragma once
#include "stable.h"

#include "frame_context.h"

#include <meta/comptr.h>
#include <meta/tuple.h>

#include <optional>
#include <thread>
#include <vector>

struct captured_update;

struct Unexpected {
    const char *text{};
};

struct Expected {
    const char *text{};
};

/// returns the global thread handle (usable in any thread!)
HANDLE GetCurrentThreadHandle();

struct capture_thread {
    struct callbacks { // not implemented here! (see application.cpp)
        void setError(std::exception_ptr error);
        void setFrame(captured_update &&frame, const frame_context &context, size_t thread_index);
    };
    struct init_args {
        callbacks *callbacks{}; // callbacks
        int display{}; // index of the display to capture
        size_t threadIndex{}; // identifier to this thread
    };
    struct start_args {
        ComPtr<ID3D11Device> device; // device used for capturing
        POINT offset{}; // offset from desktop to target coordinates
    };

    capture_thread(init_args &&args) noexcept
        : callbacks_m(args.callbacks)
        , display_m(args.display)
        , index_m(args.threadIndex) {}

    std::thread start(start_args &&args); // start a stopped thread

    void next(); // thread starts to capture the next frame
    void stop(); // signal thread to stop, use join to wait!

private:
    static void WINAPI stopAPC(ULONG_PTR);
    static void WINAPI nextAPC(ULONG_PTR);

    void run();

    void initDuplication();
    void
    handleDeviceError(const char *text, HRESULT result, std::initializer_list<HRESULT> expected);

    std::optional<captured_update> captureUpdate();

private:
    callbacks *callbacks_m;
    int display_m;
    size_t index_m;
    frame_context context_m{};
    ComPtr<ID3D11Device> device_m;
    HANDLE threadHandle_m{};
    bool keepRunning_m = true;
    bool doCapture_m = true;

    ComPtr<IDXGIOutputDuplication> dupl_m;
};
