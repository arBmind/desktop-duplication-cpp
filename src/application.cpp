#include "application.h"

#include "capture_thread.h"
#include "captured_update.h"
#include "frame_updater.h"
#include "pointer_updater.h"
#include "renderer.h"
#include "window_renderer.h"

#include "win32/power_request.h"
#include "win32/taskbar_list.h"

#include "meta/scope_guard.h"
#include "meta/tuple.h"

#include <gsl.h>
#include <windowsx.h>

namespace {
constexpr const auto WINDOM_CLASS_NAME = L"desdup";

RECT WindowWorkArea() noexcept {
    RECT result;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &result, 0);
    return result;
}

bool IsWindowMaximized(HWND windowHandle) noexcept {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(WINDOWPLACEMENT);
    const auto success = GetWindowPlacement(windowHandle, &placement);
    return success && placement.showCmd == SW_MAXIMIZE;
}

void ShowWindowBorder(HWND windowHandle, bool shown) noexcept {
    auto style = GetWindowLong(windowHandle, GWL_STYLE);
    const auto flags = WS_BORDER | WS_SIZEBOX | WS_DLGFRAME;
    if (shown) {
        style |= flags;
    }
    else {
        style &= ~flags;
    }

    SetWindowLong(windowHandle, GWL_STYLE, style);
}
uint32_t GetModifierKeys() noexcept {
    auto output = 0u;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) output |= MK_SHIFT;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) output |= MK_CONTROL;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) output |= MK_ALT;
    return output;
}

enum ThumbButton { Maximized, Freezed, VisibleArea };

} // namespace

struct internal : capture_thread::callbacks {
    using config = application::config;

    internal(config &&cfg)
        : config_m(std::move(cfg))
        , processId_m(::GetCurrentProcessId()) {}

    static auto extractApp(HWND window, UINT message, LPARAM lParam) {
        switch (message) {
        case WM_NCCREATE: {
#pragma warning(disable : 26490)
            auto create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
#pragma warning(disable : 26471)
            const auto app = gsl::not_null<internal *>{
                reinterpret_cast<internal *>(create_struct->lpCreateParams)};
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app.get()));
            app->handleWindowSetup(window);
            return app.get();
        }
        case WM_NCDESTROY: {
            auto app = reinterpret_cast<internal *>(GetWindowLongPtr(window, GWLP_USERDATA));
            if (app && app->windowHandle_m == window) app->windowHandle_m = {};
            return app;
        }
        default: return reinterpret_cast<internal *>(GetWindowLongPtr(window, GWLP_USERDATA));
        }
    }

    static LRESULT CALLBACK
    staticWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto app = extractApp(window, message, lParam);
        if (app) return app->windowProc(window, message, wParam, lParam);
        return ::DefWindowProc(window, message, wParam, lParam);
    }

    bool armed = false;
    bool dragging = false;
    bool freezed = false;
    LPARAM lastpos;
    LRESULT windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        const auto fallback = [&]() noexcept {
            return ::DefWindowProc(window, message, wParam, lParam);
        };
        if (processId_m != GetCurrentProcessId()) return fallback();

        if (message == taskbarCreatedMessage_m) {
            createTaskbarToolbar();
            return 0;
        }

        switch (message) {
        case WM_CLOSE:
            if (windowHandle_m == window) PostQuitMessage(0);
            break;

        case WM_SIZE:
            if (windowHandle_m == window) {
                handleSizeChanged(
                    SIZE{LOWORD(lParam), HIWORD(lParam)}, reinterpret_cast<uint32_t &>(wParam));
            }
            break;

        case WM_LBUTTONDBLCLK:
            toggleMaximized();
            break;

            // Zoom
        case WM_MOUSEWHEEL:
            if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)) {
                auto wheel_delta =
                    static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / (30 * WHEEL_DELTA);
                if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT)) {
                    wheel_delta /= 10.f;
                }
                changeZoom(wheel_delta);
            }
            break;

            // Offset Dragging
        case WM_LBUTTONDOWN:
            if (GET_KEYSTATE_WPARAM(wParam) == (MK_LBUTTON | MK_SHIFT)) {
                dragging = true;
                lastpos = lParam;
            }
            break;
        case WM_LBUTTONUP: dragging = false; break;
        case WM_MOUSEMOVE:
            if (dragging && GET_KEYSTATE_WPARAM(wParam) == (MK_LBUTTON | MK_SHIFT)) {
                const auto delta = POINT{GET_X_LPARAM(lParam) - GET_X_LPARAM(lastpos),
                                         GET_Y_LPARAM(lParam) - GET_Y_LPARAM(lastpos)};
                moveTexture(delta);
                lastpos = lParam;
            }
            break;

            // Fit Other Window
        case WM_RBUTTONDBLCLK: armed ^= true; break;
        case WM_KILLFOCUS:
            if (armed) {
                armed = false;
                auto other_window = GetForegroundWindow();
                fitWindow(other_window);
            }
            break;

            // Keyboard
        case WM_KEYDOWN: {
            const auto mk = GetModifierKeys();
            switch (wParam) {
            case VK_UP:
                if (mk == MK_SHIFT) moveTextureTo(0, -1);
                break;
            case VK_DOWN:
                if (mk == MK_SHIFT) moveTextureTo(0, +1);
                break;
            case VK_LEFT:
                if (mk == MK_SHIFT) moveTextureTo(-1, 0);
                break;
            case VK_RIGHT:
                if (mk == MK_SHIFT) moveTextureTo(+1, 0);
                break;
            default: {
                const auto ch =
                    MapVirtualKey(reinterpret_cast<uint32_t &>(wParam), MAPVK_VK_TO_CHAR);
                switch (ch) {
                case '0':
                    if (mk == MK_CONTROL)
                        setZoom(1.f);
                    else if (mk & MK_CONTROL)
                        setZoom(1.f);
                    break;
                case '+':
                    if (mk == MK_CONTROL)
                        changeZoom(0.01f);
                    else if (mk & MK_CONTROL)
                        changeZoom(0.001f);
                    break;
                case '-':
                    if (mk == MK_CONTROL)
                        changeZoom(-0.01f);
                    else if (mk & MK_CONTROL)
                        changeZoom(-0.001f);
                    break;
                case 'F': // Freeze
                case 'P': // Pause / Play
                    if (mk == MK_CONTROL) {
                        toggleFreezed();
                    }
                } // switch
            }
            } // switch
            break;
        }
            // DPI Aware
        case WM_DPICHANGED: {
            const auto new_rect = gsl::not_null<LPRECT>{reinterpret_cast<LPRECT>(lParam)};
            const auto new_size = rectSize(*new_rect);
            MoveWindow(window, new_rect->left, new_rect->top, new_size.cx, new_size.cy, true);
            break;
        }
            // Visible Area
        case WM_RBUTTONUP:
            if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT)) {
                toggleVisibleArea();
            }
            break;
        case WM_COMMAND: {
            const auto command = LOWORD(wParam);
            const auto handled = handleCommand(command);
            if (handled) break;
            return fallback();
        }
        case WM_PAINT:
            if (window == visibleAreaWindow_m) {
                paintVisibleArea();
                break;
            }
            // fall through
        default: return fallback();
        }
        return 0;
    }

    void handleWindowSetup(HWND window) {
        if (!windowHandle_m) {
            windowHandle_m = window;
            canStartDuplication_m = true;
            taskbarList_m = decltype(taskbarList_m)(window);
            toggleVisibleArea();
        }
    }

    bool haveTaskbar = false;
    void createTaskbarToolbar() {
        haveTaskbar = true;
        taskbarList_m.setButtonFlags(ThumbButton::Maximized, win32::ThumbButtonFlag::Enabled);
        taskbarList_m.setButtonTooltip(ThumbButton::Maximized, L"toggle fullscreen");
        visualizeMaximized();

        taskbarList_m.setButtonFlags(ThumbButton::Freezed, win32::ThumbButtonFlag::Enabled);
        taskbarList_m.setButtonTooltip(ThumbButton::Freezed, L"toggle freezed");
        visualizeFreezed();

        taskbarList_m.setButtonFlags(ThumbButton::VisibleArea, win32::ThumbButtonFlag::Enabled);
        taskbarList_m.setButtonTooltip(ThumbButton::VisibleArea, L"toggle area");
        visualizeVisibleArea();

        taskbarList_m.updateThumbButtons();
    }

    bool handleCommand(int command) {
        switch (command) {
        case ThumbButton::Maximized: toggleMaximized(); return true;
        case ThumbButton::Freezed: toggleFreezed(); return true;
        case ThumbButton::VisibleArea: toggleVisibleArea(); return true;
        }
        return false;
    }

    void toggleFreezed() {
        freezed ^= 1;
        if (haveTaskbar) {
            visualizeFreezed();
            taskbarList_m.updateThumbButtons();
        }
        if (!freezed) captureNextFrame();
    }
    void visualizeFreezed() {
        if (freezed) {
            taskbarList_m.setProgressFlags(win32::ProgressFlag::Paused);
            taskbarList_m.setProgressValue(1, 1);
            taskbarList_m.setButtonLetterIcon(1, 0xE768, RGB(100, 255, 100));
        }
        else {
            taskbarList_m.setProgressFlags(win32::ProgressFlag::Normal);
            taskbarList_m.setProgressValue(1, 1);
            taskbarList_m.setButtonLetterIcon(1, 0xE103, RGB(255, 200, 10));
        }
    }

    constexpr static auto colorKey = RGB(0xFF, 0x20, 0xFF);
    void toggleVisibleArea() {
        if (nullptr == visibleAreaWindow_m) {
            {
                const DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST;
                const auto style = WS_POPUP;
                const auto hasMenu = false;

                auto rect = visibleAreaRect();
                AdjustWindowRectEx(&rect, style, hasMenu, exStyle);
                const auto w = 4 + rect.right - rect.left, h = 4 + rect.bottom - rect.top,
                           x = rect.left - 2, y = rect.top - 2;

                const auto windowName = nullptr;
                const auto parentWindow = windowHandle_m;
                const auto menu = HMENU{};
                const auto customParam = this;
                const auto instance = GetModuleHandle(nullptr);
                visibleAreaWindow_m = CreateWindowExW(
                    exStyle,
                    WINDOM_CLASS_NAME,
                    windowName,
                    style,
                    x,
                    y,
                    w,
                    h,
                    parentWindow,
                    menu,
                    instance,
                    customParam);
            }
            {
                const BYTE alpha = 0u;
                const BYTE flags = LWA_COLORKEY;
                SetLayeredWindowAttributes(visibleAreaWindow_m, colorKey, alpha, flags);
            }
            ShowWindow(visibleAreaWindow_m, SW_SHOW);
            UpdateWindow(visibleAreaWindow_m);
        }
        else {
            DestroyWindow(visibleAreaWindow_m);
            visibleAreaWindow_m = nullptr;
        }
        if (haveTaskbar) {
            visualizeVisibleArea();
            taskbarList_m.updateThumbButtons();
        }
    }

    void visualizeVisibleArea() noexcept {
        if (visibleAreaWindow_m) {
            taskbarList_m.setButtonLetterIcon(ThumbButton::VisibleArea, 0xEF20, RGB(128, 128, 128));
        }
        else {
            taskbarList_m.setButtonLetterIcon(ThumbButton::VisibleArea, 0xEF20, colorKey);
        }
    }

    void updateVisibleArea() noexcept {
        if (nullptr == visibleAreaWindow_m) return;

        const auto rect = visibleAreaRect();
        const auto size = rectSize(rect);
        const auto repaint = true;
        MoveWindow(
            visibleAreaWindow_m, rect.left - 2, rect.top - 2, size.cx + 4, size.cy + 4, repaint);
    }

    void paintVisibleArea() noexcept {
        RECT window_rect;
        GetClientRect(visibleAreaWindow_m, &window_rect);
        const auto window_size = rectSize(window_rect);

        PAINTSTRUCT p;
        auto dc = BeginPaint(visibleAreaWindow_m, &p);

        SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        SelectObject(dc, GetStockObject(DC_PEN));

        SetDCPenColor(dc, RGB(255, 80, 40));
        Rectangle(dc, 0, 0, window_size.cx, window_size.cy);

        SelectObject(dc, GetStockObject(DC_BRUSH));
        SetDCBrushColor(dc, colorKey);
        SetDCPenColor(dc, RGB(255, 255, 255));
        Rectangle(dc, 1, 1, window_size.cx - 1, window_size.cy - 1);

        EndPaint(visibleAreaWindow_m, &p);
    }

    void handleSizeChanged(const SIZE &, const uint32_t flags) noexcept {
        if (!duplicationStarted_m) return;
        const bool maximized = (flags & SIZE_MAXIMIZED) != 0;
        togglePowerRequest(maximized);
        RECT window_rect{};
        if (maximized)
            GetWindowRect(windowHandle_m, &window_rect);
        else
            GetClientRect(windowHandle_m, &window_rect);
        const auto window_size = rectSize(window_rect);
        const auto changed = windowRenderer_m.resize(window_size);
        if (changed) {
            doRender_m = true;
            updateVisibleArea();
        }
    }

    void togglePowerRequest(bool enable) noexcept {
        const auto success = enable ? powerRequest_m.set() : powerRequest_m.clear();
        if (!success) {
            OutputDebugStringA("Power Request Failed!\n");
        }
    }

    void changeZoom(float zoomDelta) noexcept {
        setZoom(windowRenderer_m.zoom() + zoomDelta);
        updateVisibleArea();
    }
    void setZoom(float zoom) noexcept {
        if (!duplicationStarted_m) return;
        windowRenderer_m.setZoom(std::max(zoom, 0.05f));
        updateVisibleArea();
        doRender_m = true;
    }

    void toggleMaximized() {
        ::ShowWindow(
            windowHandle_m, IsWindowMaximized(windowHandle_m) ? SW_SHOWNORMAL : SW_MAXIMIZE);
        updateVisibleArea();
        if (haveTaskbar) {
            visualizeMaximized();
            taskbarList_m.updateThumbButtons();
        }
    }
    void visualizeMaximized() noexcept {
        if (IsWindowMaximized(windowHandle_m)) {
            taskbarList_m.setButtonLetterIcon(ThumbButton::Maximized, 0xE923);
        }
        else {
            taskbarList_m.setButtonLetterIcon(ThumbButton::Maximized, 0xE922);
        }
    }

    void moveTexture(POINT delta) noexcept {
        windowRenderer_m.moveOffset(delta);
        updateVisibleArea();
        doRender_m = true;
    }
    void moveTextureTo(int x, int y) {
        windowRenderer_m.moveToBorder(x, y);
        updateVisibleArea();
        doRender_m = true;
    }

    RECT visibleAreaRect() noexcept {
        RECT window_rect;
        GetClientRect(windowHandle_m, &window_rect);
        const auto window_size = rectSize(window_rect);
        const auto zoom = windowRenderer_m.zoom();
        const auto renderOffset = windowRenderer_m.offset();
        const auto left = offset_m.x - renderOffset.x;
        const auto top = offset_m.y - renderOffset.y;
        const auto width = static_cast<long>(window_size.cx / zoom);
        const auto height = static_cast<long>(window_size.cy / zoom);
        return RECT{left, top, left + width, top + height};
    }

    void fitWindow(HWND otherWindow) noexcept {
        RECT other_rect;
        GetWindowRect(otherWindow, &other_rect);
        const auto rect = visibleAreaRect();
        const auto size = rectSize(rect);
        const auto repaint = true;
        MoveWindow(otherWindow, rect.left, rect.top, size.cx, size.cy, repaint);
    }

    static auto registerWindowClass(HINSTANCE instanceHandle) -> ATOM {
        const auto cursor = LoadCursor(nullptr, IDC_CROSS);
        if (!cursor) throw Unexpected{"LoadCursor failed"};
        LATER(DestroyCursor(cursor));

        const auto icon = LoadIconW(instanceHandle, L"desk1");

        WNDCLASSEXW window_class;
        window_class.cbSize = sizeof(WNDCLASSEXW);
        window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        window_class.lpfnWndProc = staticWindowProc;
        window_class.cbClsExtra = 0;
        window_class.cbWndExtra = 0;
        window_class.hInstance = instanceHandle;
        window_class.hIcon = icon;
        window_class.hCursor = cursor;
        window_class.hbrBackground = nullptr;
        window_class.lpszMenuName = nullptr;
        window_class.lpszClassName = WINDOM_CLASS_NAME;
        window_class.hIconSm = icon;

        const auto atom = RegisterClassExW(&window_class);
        if (!atom) throw Unexpected{"Window class registration failed"};
        return atom;
    }

    HWND createMainWindow(HINSTANCE instanceHandle, int showCommand) {
        const auto wA = WindowWorkArea();
        const auto wS = rectSize(wA);
        const auto rect = RECT{wA.right - wS.cx / 2, 0, wA.right, wA.bottom - wS.cy / 2};
        const DWORD style = WS_OVERLAPPEDWINDOW;
        const auto exStyle = DWORD{};
        const auto hasMenu = false;
        // AdjustWindowRectEx(&rect, style, hasMenu, exStyle);

        static constexpr const auto title =
            L"Duplicate Desktop Presenter (Double Click to toggle "
            L"fullscreen)";
        const auto size = rectSize(rect);
        const auto customParam = this;
        const auto window = CreateWindowExW(
            exStyle,
            WINDOM_CLASS_NAME,
            title,
            style,
            rect.left,
            rect.top,
            size.cx,
            size.cy,
            HWND{},
            HMENU{},
            instanceHandle,
            customParam);
        if (!window) throw Unexpected{"Window creation failed"};

        ShowWindow(window, showCommand);
        UpdateWindow(window);

        return window;
    }

    static void CALLBACK setErrorAPC(ULONG_PTR parameter) {
        auto args = unique_tuple_ptr<internal *, std::exception_ptr>(parameter);
        std::get<internal *>(*args)->updateError(std::get<1>(*args));
    }
    static void CALLBACK setFrameAPC(ULONG_PTR parameter) {
        auto args = unique_tuple_ptr<
            internal *,
            captured_update,
            std::reference_wrapper<frame_context>,
            size_t>(parameter);
        std::get<internal *>(*args)->updateFrame(
            std::get<1>(*args), std::get<2>(*args), std::get<3>(*args));
    }
    static void CALLBACK
    retryDuplicationAPC(LPVOID parameter, DWORD dwTimerLowValue, DWORD dwTimerHighValue) noexcept {
        auto self = static_cast<internal *>(parameter);
        self->canStartDuplication_m = true;
    }
    static void CALLBACK
    awaitFrameAPC(LPVOID parameter, DWORD dwTimerLowValue, DWORD dwTimerHighValue) noexcept {
        auto self = static_cast<internal *>(parameter);
        self->waitFrame_m = false;
    }

    void initCaptureThreads() {
        for (const auto display : config_m.displays) {
            capture_thread::init_args args;
            args.callbacks = this;
            args.display = display;
            args.threadIndex = captureThreads_m.size();
            captureThreads_m.emplace_back(std::make_unique<capture_thread>(std::move(args)));
        }
        threads_m.reserve(config_m.displays.size());
    }

    int run() {
        try {
            registerWindowClass(config_m.instanceHandle);
            taskbarCreatedMessage_m = RegisterWindowMessageW(L"TaskbarButtonCreated");

            ChangeWindowMessageFilter(taskbarCreatedMessage_m, MSGFLT_ADD);
            ChangeWindowMessageFilter(WM_COMMAND, MSGFLT_ADD);
            threadHandle_m = GetCurrentThreadHandle();
            LATER(CloseHandle(threadHandle_m));
            powerRequest_m = decltype(powerRequest_m)(L"Desktop Duplication Tool");
            retryTimer_m = CreateWaitableTimer(nullptr, false, TEXT("RetryTimer"));
            LATER(CloseHandle(retryTimer_m));
            frameTimer_m = CreateWaitableTimer(nullptr, false, TEXT("FrameTimer"));
            LATER(CloseHandle(frameTimer_m));

            createMainWindow(config_m.instanceHandle, config_m.showCommand);
            initCaptureThreads();

            return mainLoop();
        }
        catch (const Unexpected &e) {
            OutputDebugStringA(e.text);
            OutputDebugStringA("\n");
            return -1;
        }
    }

    int mainLoop() {
        while (keepRunning_m) {
            try {
                if (!canStartDuplication_m) {
                    sleep();
                    continue;
                }
                startDuplication();
                taskbarList_m.setProgressFlags(win32::ProgressFlag::Normal);
                taskbarList_m.setProgressValue(1, 1);
            }
            catch (Expected &e) {
                OutputDebugStringA(e.text);
                OutputDebugStringA("\n");
                taskbarList_m.setProgressFlags(win32::ProgressFlag::Error);
                taskbarList_m.setProgressValue(1, 3);
                stopDuplication();
                awaitRetry();
                continue;
            }
            try {
                while (keepRunning_m && duplicationStarted_m && !hasError_m) {
                    render();
                    sleep();
                }
                rethrow();
            }
            catch (Expected &e) {
                OutputDebugStringA(e.text);
                OutputDebugStringA("\n");
                taskbarList_m.setProgressFlags(win32::ProgressFlag::Error);
                taskbarList_m.setProgressValue(2, 3);
                error_m = {};
                stopDuplication();
            }
        }
        stopDuplication();
        return returnValue_m;
    }

    void startDuplication() {
        if (duplicationStarted_m) return;
        duplicationStarted_m = true;
        try {
            auto deviceValue = renderer::createDevice();
            auto device = deviceValue.device;
            auto deviceContext = deviceValue.deviceContext;

            auto dimensions = renderer::getDimensionData(device, config_m.displays);

            target_m = renderer::createSharedTexture(device, rectSize(dimensions.rect));
            windowRenderer_m.init([&]() noexcept {
                auto args = window_renderer::init_args{};
                args.windowHandle = windowHandle_m;
                args.texture = target_m;
                args.device = device;
                args.deviceContext = deviceContext;
                return args;
            }());
            offset_m = POINT{dimensions.rect.left, dimensions.rect.top};
            auto handle = renderer::getSharedHandle(target_m);
            for (const auto &capture : captureThreads_m) {
                startCaptureThread(*capture, offset_m, handle);
            }
        }
        catch (const renderer::error &e) {
            throw Expected{e.message};
        }
    }

    void startCaptureThread(capture_thread &capture, const POINT &offset, HANDLE targetHandle) {
        auto deviceValue = renderer::createDevice();
        auto device = deviceValue.device;
        auto deviceContext = deviceValue.deviceContext;

        frame_updater::init_args updater_args;
        updater_args.device = device;
        updater_args.deviceContext = deviceContext;
        updater_args.targetHandle = targetHandle;
        frameUpdaters_m.emplace_back(std::move(updater_args));

        capture_thread::start_args threadArgs;
        threadArgs.device = device;
        threadArgs.offset = offset;
        threads_m.emplace_back(capture.start(std::move(threadArgs)));
    }

    void stopDuplication() {
        if (!duplicationStarted_m) return;
        try {
            for (const auto &capture : captureThreads_m) {
                capture->stop();
            }
            for (auto &thread : threads_m) {
                thread.join();
            }
            threads_m.clear();
            target_m.Reset();
            frameUpdaters_m.clear();
            updatedThreads_m.clear();
            windowRenderer_m.reset();
        }
        catch (Expected &e) {
            OutputDebugStringA("stopDuplication Threw: ");
            OutputDebugStringA(e.text);
            OutputDebugStringA("\n");
        }
        duplicationStarted_m = false;
    }

    void render() {
        if (waitFrame_m || !doRender_m) return;
        doRender_m = false;
        ShowWindowBorder(windowHandle_m, !IsWindowMaximized(windowHandle_m));
        try {
            windowRenderer_m.render();
            windowRenderer_m.renderPointer(pointerUpdater_m.data());
            windowRenderer_m.swap();
            captureNextFrame();
            awaitNextFrame();
            updateVisibleArea();
        }
        catch (const renderer::error &e) {
            throw Expected{e.message};
        }
    }

    void captureNextFrame() {
        if (freezed) return;
        for (auto index : updatedThreads_m) captureThreads_m[index]->next();
        updatedThreads_m.clear();
    }

    void awaitNextFrame() {
        if (!waitFrame_m) {
            waitFrame_m = true;
            LARGE_INTEGER queueTime;
            queueTime.QuadPart = -10 * 1000 * 10; // relative 10 ms
            const auto noPeriod = 0;
            const auto apcArgument = this;
            const auto awakeSuspend = false;
            const auto success = SetWaitableTimer(
                retryTimer_m, &queueTime, noPeriod, awaitFrameAPC, apcArgument, awakeSuspend);
            if (!success) throw Unexpected{"failed to arm frame timer"};
        }
    }

    void awaitRetry() {
        if (canStartDuplication_m) {
            canStartDuplication_m = false;
            LARGE_INTEGER queueTime;
            queueTime.QuadPart = -250 * 1000 * 10; // relative 250 ms
            const auto noPeriod = 0;
            const auto apcArgument = this;
            const auto awakeSuspend = false;
            const auto success = SetWaitableTimer(
                frameTimer_m, &queueTime, noPeriod, retryDuplicationAPC, apcArgument, awakeSuspend);
            if (!success) throw Unexpected{"failed to arm retry timer"};
        }
    }

    void sleep(unsigned int timeout = INFINITE) {
        const auto wake_mask = QS_ALLINPUT;
        const auto flags = MWMO_ALERTABLE | MWMO_INPUTAVAILABLE;
        const auto handle_count = handles_m.size();
        const auto awoken = MsgWaitForMultipleObjectsEx(
            reinterpret_cast<const uint32_t &>(handle_count),
            handles_m.data(),
            timeout,
            wake_mask,
            flags);
        if (WAIT_FAILED == awoken) throw Unexpected{"Application Waiting failed"};
        if (WAIT_OBJECT_0 == awoken - handle_count) processMessages();
    }

    void processMessages() noexcept {
        while (true) {
            auto message = MSG{};
            const auto success = PeekMessage(&message, nullptr, 0, 0, PM_REMOVE);
            if (!success) return;
            TranslateMessage(&message);
            DispatchMessage(&message);
            if (WM_QUIT == message.message) {
                keepRunning_m = false;
                returnValue_m = reinterpret_cast<int &>(message.wParam);
            }
        }
    }

    void rethrow() {
        if (!hasError_m) return;
        hasError_m = false;
        std::rethrow_exception(error_m);
    }

    void updateError(const std::exception_ptr &exception) noexcept {
        error_m = exception;
        hasError_m = true;
    }
    void updateFrame(captured_update &update, const frame_context &context, size_t threadIndex) {
        if (!duplicationStarted_m) return;
        frameUpdaters_m[threadIndex].update(update.frame, context);
        pointerUpdater_m.update(update.pointer, context);
        updatedThreads_m.push_back(threadIndex);
        doRender_m = true;
    }

private:
    config config_m;
    DWORD processId_m{};
    HWND windowHandle_m{};
    UINT taskbarCreatedMessage_m{};
    win32::power_request<PowerRequestDisplayRequired, PowerRequestSystemRequired> powerRequest_m;
    win32::taskbar_list taskbarList_m;
    HANDLE retryTimer_m{};
    HANDLE frameTimer_m{};

public:
    HANDLE threadHandle_m;

private:
    bool canStartDuplication_m = false;
    bool duplicationStarted_m = false;
    bool keepRunning_m = true;
    int returnValue_m = 0;
    bool hasError_m = false;
    std::exception_ptr error_m;
    bool waitFrame_m = false;
    bool doRender_m = false;

    POINT offset_m;
    window_renderer windowRenderer_m;
    ComPtr<ID3D11Texture2D> target_m;
    std::vector<frame_updater> frameUpdaters_m;
    pointer_updater pointerUpdater_m;

    using capture_thread_ptr = std::unique_ptr<capture_thread>;

    std::vector<HANDLE> handles_m;
    std::vector<std::thread> threads_m;
    std::vector<capture_thread_ptr> captureThreads_m;
    std::vector<size_t> updatedThreads_m;

    HWND visibleAreaWindow_m = nullptr;
};

struct application::internal : ::internal {
    using ::internal::internal;
};

application::application(config &&cfg)
#pragma warning(disable : 26409) // make_unique allows no custom deleter
    : ip(new internal(std::move(cfg))) {
}

int application::run() { return ip->run(); }

[[gsl::suppress(26462)]] // type is not expressable
    void
    capture_thread::callbacks::setError(std::exception_ptr error) {
        auto self = reinterpret_cast<internal *>(this);

        const auto parameter = make_tuple_ptr(self, error);
        const auto success = QueueUserAPC(
            internal::setErrorAPC, self->threadHandle_m, reinterpret_cast<ULONG_PTR>(parameter));
        if (!success) throw Unexpected{"api::setError failed to queue APC"};
    }

        [[gsl::suppress(26462)]] // type is not expressable
    void capture_thread::callbacks::setFrame(
        captured_update &&frame, const frame_context &context, size_t thread_index) {

    auto self = reinterpret_cast<internal *>(this);

    const auto parameter = make_tuple_ptr(self, std::move(frame), std::ref(context), thread_index);
    const auto success = QueueUserAPC(
        internal::setFrameAPC, self->threadHandle_m, reinterpret_cast<ULONG_PTR>(parameter));
    if (!success) throw Unexpected{"api::setError failed to queue APC"};
}

void application::internal_deleter::operator()(internal *ptr) noexcept { delete ptr; }
