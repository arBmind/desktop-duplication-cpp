#include "application.h"

#include "CaptureThread.h"
#include "CapturedUpdate.h"
#include "FrameUpdater.h"
#include "PointerUpdater.h"
#include "WindowRenderer.h"
#include "renderer.h"

#include "win32/PowerRequest.h"
#include "win32/TaskbarList.h"

#include "meta/scope_guard.h"
#include "meta/tuple.h"

#include <gsl.h>
#include <windowsx.h>

namespace {
constexpr const auto WINDOM_CLASS_NAME = L"desdup";

auto WindowWorkArea() noexcept -> RECT {
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
auto GetModifierKeys() noexcept -> uint32_t {
    auto output = 0u;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) output |= MK_SHIFT;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) output |= MK_CONTROL;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) output |= MK_ALT;
    return output;
}

enum ThumbButton { Maximized, Freezed, VisibleArea };

struct AppImpl : CaptureThread::Callbacks {
    using Config = Application::Config;

    AppImpl(Config &&config)
        : m_config(std::move(config))
        , m_processId(::GetCurrentProcessId()) {}

    static auto extractApp(HWND window, UINT message, LPARAM lParam) -> AppImpl * {
        switch (message) {
        case WM_NCCREATE: {
            auto create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            const auto app = static_cast<AppImpl *>(create_struct->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            app->handleWindowSetup(window);
            return app;
        }
        case WM_NCDESTROY: {
            auto app = reinterpret_cast<AppImpl *>(GetWindowLongPtr(window, GWLP_USERDATA));
            if (app && app->m_windowHandle == window) app->m_windowHandle = {};
            return app;
        }
        default: return reinterpret_cast<AppImpl *>(GetWindowLongPtr(window, GWLP_USERDATA));
        }
    }

    static auto CALLBACK staticWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        -> LRESULT {
        auto app = extractApp(window, message, lParam);
        if (app) return app->windowProc(window, message, wParam, lParam);
        return ::DefWindowProc(window, message, wParam, lParam);
    }

    bool m_isWindowPicking = false;
    bool m_isDragging = false;
    bool m_isFreezed = false;
    LPARAM m_lastpos = {};

    auto windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
        const auto fallback = [&]() noexcept {
            return ::DefWindowProc(window, message, wParam, lParam);
        };
        if (m_processId != GetCurrentProcessId()) return fallback();

        if (message == m_taskbarCreatedMessage) {
            createTaskbarToolbar();
            return 0;
        }

        switch (message) {
        case WM_CLOSE:
            if (m_windowHandle == window) PostQuitMessage(0);
            break;

        case WM_SIZE:
            if (m_windowHandle == window) {
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
                m_isDragging = true;
                m_lastpos = lParam;
            }
            break;
        case WM_LBUTTONUP: m_isDragging = false; break;
        case WM_MOUSEMOVE:
            if (m_isDragging && GET_KEYSTATE_WPARAM(wParam) == (MK_LBUTTON | MK_SHIFT)) {
                const auto delta = POINT{
                    GET_X_LPARAM(lParam) - GET_X_LPARAM(m_lastpos),
                    GET_Y_LPARAM(lParam) - GET_Y_LPARAM(m_lastpos)};
                moveTexture(delta);
                m_lastpos = lParam;
            }
            break;

            // Fit Other Window
        case WM_RBUTTONDBLCLK: m_isWindowPicking ^= true; break;
        case WM_KILLFOCUS:
            if (m_isWindowPicking) {
                m_isWindowPicking = false;
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
            if (window == m_visibleAreaWindow) {
                paintVisibleArea();
                break;
            }
            [[fallthrough]];
        default: return fallback();
        }
        return 0;
    }

    void handleWindowSetup(HWND window) {
        if (!m_windowHandle) {
            m_windowHandle = window;
            m_canStartDuplication = true;
            m_taskbarList = decltype(m_taskbarList)(window);
            toggleVisibleArea();
        }
    }

    bool m_haveTaskbar = false;
    void createTaskbarToolbar() {
        m_haveTaskbar = true;
        m_taskbarList.setButtonFlags(ThumbButton::Maximized, win32::ThumbButtonFlag::Enabled);
        m_taskbarList.setButtonTooltip(ThumbButton::Maximized, L"toggle fullscreen");
        visualizeMaximized();

        m_taskbarList.setButtonFlags(ThumbButton::Freezed, win32::ThumbButtonFlag::Enabled);
        m_taskbarList.setButtonTooltip(ThumbButton::Freezed, L"toggle freezed");
        visualizeFreezed();

        m_taskbarList.setButtonFlags(ThumbButton::VisibleArea, win32::ThumbButtonFlag::Enabled);
        m_taskbarList.setButtonTooltip(ThumbButton::VisibleArea, L"toggle area");
        visualizeVisibleArea();

        m_taskbarList.updateThumbButtons();
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
        m_isFreezed ^= 1;
        if (m_haveTaskbar) {
            visualizeFreezed();
            m_taskbarList.updateThumbButtons();
        }
        if (!m_isFreezed) captureNextFrame();
    }
    void visualizeFreezed() {
        if (m_isFreezed) {
            m_taskbarList.setProgressFlags(win32::ProgressFlag::Paused);
            m_taskbarList.setProgressValue(1, 1);
            m_taskbarList.setButtonLetterIcon(1, 0xE768, RGB(100, 255, 100));
        }
        else {
            m_taskbarList.setProgressFlags(win32::ProgressFlag::Normal);
            m_taskbarList.setProgressValue(1, 1);
            m_taskbarList.setButtonLetterIcon(1, 0xE103, RGB(255, 200, 10));
        }
    }

    constexpr static auto colorKey = RGB(0xFF, 0x20, 0xFF);
    void toggleVisibleArea() {
        if (nullptr == m_visibleAreaWindow) {
            {
                const DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST;
                const auto style = WS_POPUP;
                const auto hasMenu = false;

                auto rect = visibleAreaRect();
                AdjustWindowRectEx(&rect, style, hasMenu, exStyle);
                const auto w = 4 + rect.right - rect.left, h = 4 + rect.bottom - rect.top,
                           x = rect.left - 2, y = rect.top - 2;

                const auto windowName = nullptr;
                const auto parentWindow = m_windowHandle;
                const auto menu = HMENU{};
                const auto customParam = this;
                const auto instance = GetModuleHandle(nullptr);
                m_visibleAreaWindow = CreateWindowExW(
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
                SetLayeredWindowAttributes(m_visibleAreaWindow, colorKey, alpha, flags);
            }
            ShowWindow(m_visibleAreaWindow, SW_SHOW);
            UpdateWindow(m_visibleAreaWindow);
        }
        else {
            DestroyWindow(m_visibleAreaWindow);
            m_visibleAreaWindow = nullptr;
        }
        if (m_haveTaskbar) {
            visualizeVisibleArea();
            m_taskbarList.updateThumbButtons();
        }
    }

    void visualizeVisibleArea() noexcept {
        if (m_visibleAreaWindow) {
            m_taskbarList.setButtonLetterIcon(ThumbButton::VisibleArea, 0xEF20, RGB(128, 128, 128));
        }
        else {
            m_taskbarList.setButtonLetterIcon(ThumbButton::VisibleArea, 0xEF20, colorKey);
        }
    }

    void updateVisibleArea() noexcept {
        if (nullptr == m_visibleAreaWindow) return;

        const auto rect = visibleAreaRect();
        const auto size = rectSize(rect);
        const auto repaint = true;
        MoveWindow(
            m_visibleAreaWindow, rect.left - 2, rect.top - 2, size.cx + 4, size.cy + 4, repaint);
    }

    void paintVisibleArea() noexcept {
        RECT window_rect;
        GetClientRect(m_visibleAreaWindow, &window_rect);
        const auto window_size = rectSize(window_rect);

        PAINTSTRUCT p;
        auto dc = BeginPaint(m_visibleAreaWindow, &p);

        SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        SelectObject(dc, GetStockObject(DC_PEN));

        SetDCPenColor(dc, RGB(255, 80, 40));
        Rectangle(dc, 0, 0, window_size.cx, window_size.cy);

        SelectObject(dc, GetStockObject(DC_BRUSH));
        SetDCBrushColor(dc, colorKey);
        SetDCPenColor(dc, RGB(255, 255, 255));
        Rectangle(dc, 1, 1, window_size.cx - 1, window_size.cy - 1);

        EndPaint(m_visibleAreaWindow, &p);
    }

    void handleSizeChanged(const SIZE &, const uint32_t flags) noexcept {
        if (!m_isDuplicationStarted) return;
        const bool maximized = (flags & SIZE_MAXIMIZED) != 0;
        togglePowerRequest(maximized);
        RECT window_rect{};
        if (maximized)
            GetWindowRect(m_windowHandle, &window_rect);
        else
            GetClientRect(m_windowHandle, &window_rect);
        const auto window_size = rectSize(window_rect);
        const auto changed = m_windowRenderer.resize(window_size);
        if (changed) {
            resetFrameLatencyHandle();
            m_doRender = true;
            updateVisibleArea();
        }
    }

    void resetFrameLatencyHandle() {
        if (m_frameLatencyIndex >= 0) {
            CloseHandle(m_handles[m_frameLatencyIndex]);
            m_handles.erase(m_handles.begin() + m_frameLatencyIndex);
            m_frameLatencyIndex = -1;
            m_waitFrame = false;
        }
    }

    void togglePowerRequest(bool enable) noexcept {
        const auto success = enable ? m_powerRequest.set() : m_powerRequest.clear();
        if (!success) {
            OutputDebugStringA("Power Request Failed!\n");
        }
    }

    void changeZoom(float zoomDelta) noexcept {
        setZoom(m_windowRenderer.zoom() + zoomDelta);
        updateVisibleArea();
    }
    void setZoom(float zoom) noexcept {
        if (!m_isDuplicationStarted) return;
        m_windowRenderer.setZoom(std::max(zoom, 0.05f));
        updateVisibleArea();
        m_doRender = true;
    }

    void toggleMaximized() {
        ::ShowWindow(
            m_windowHandle, IsWindowMaximized(m_windowHandle) ? SW_SHOWNORMAL : SW_MAXIMIZE);
        updateVisibleArea();
        if (m_haveTaskbar) {
            visualizeMaximized();
            m_taskbarList.updateThumbButtons();
        }
    }
    void visualizeMaximized() noexcept {
        if (IsWindowMaximized(m_windowHandle)) {
            m_taskbarList.setButtonLetterIcon(ThumbButton::Maximized, 0xE923);
        }
        else {
            m_taskbarList.setButtonLetterIcon(ThumbButton::Maximized, 0xE922);
        }
    }

    void moveTexture(POINT delta) noexcept {
        m_windowRenderer.moveOffset(delta);
        updateVisibleArea();
        m_doRender = true;
    }
    void moveTextureTo(int x, int y) {
        m_windowRenderer.moveToBorder(x, y);
        updateVisibleArea();
        m_doRender = true;
    }

    RECT visibleAreaRect() noexcept {
        RECT window_rect;
        GetClientRect(m_windowHandle, &window_rect);
        const auto window_size = rectSize(window_rect);
        const auto zoom = m_windowRenderer.zoom();
        const auto renderOffset = m_windowRenderer.offset();
        const auto left = m_offset.x - renderOffset.x;
        const auto top = m_offset.y - renderOffset.y;
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

    auto createMainWindow(HINSTANCE instanceHandle, int showCommand) -> HWND {
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
        auto args = unique_tuple_ptr_cast<AppImpl *, std::exception_ptr>(parameter);
        std::get<AppImpl *>(*args)->updateError(std::get<1>(*args));
    }
    static void CALLBACK setFrameAPC(ULONG_PTR parameter) {
        auto args = unique_tuple_ptr_cast<
            AppImpl *,
            CapturedUpdate,
            std::reference_wrapper<FrameContext>,
            size_t>(parameter);
        std::get<AppImpl *>(*args)->updateFrame(
            std::get<1>(*args), std::get<2>(*args), std::get<3>(*args));
    }
    static void CALLBACK
    retryDuplicationAPC(LPVOID parameter, DWORD dwTimerLowValue, DWORD dwTimerHighValue) noexcept {
        auto self = static_cast<AppImpl *>(parameter);
        self->m_canStartDuplication = true;
    }

    void initCaptureThreads() {
        for (const auto display : m_config.displays) {
            CaptureThread::InitArgs args;
            args.callbacks = this;
            args.display = display;
            args.threadIndex = m_captureThreads.size();
            m_captureThreads.emplace_back(std::make_unique<CaptureThread>(std::move(args)));
        }
        m_threads.reserve(m_config.displays.size());
    }

    int run() {
        try {
            registerWindowClass(m_config.instanceHandle);
            m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarButtonCreated");

            ChangeWindowMessageFilter(m_taskbarCreatedMessage, MSGFLT_ADD);
            ChangeWindowMessageFilter(WM_COMMAND, MSGFLT_ADD);
            m_threadHandle = GetCurrentThreadHandle();
            LATER(CloseHandle(m_threadHandle));
            m_powerRequest = decltype(m_powerRequest)(L"Desktop Duplication Tool");
            m_retryTimer = CreateWaitableTimer(nullptr, false, TEXT("RetryTimer"));
            LATER(CloseHandle(m_retryTimer));

            createMainWindow(m_config.instanceHandle, m_config.showCommand);
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
        while (m_keepRunning) {
            try {
                if (!m_canStartDuplication) {
                    sleep();
                    continue;
                }
                startDuplication();
                m_taskbarList.setProgressFlags(win32::ProgressFlag::Normal);
                m_taskbarList.setProgressValue(1, 1);
            }
            catch (Expected &e) {
                OutputDebugStringA(e.text);
                OutputDebugStringA("\n");
                m_taskbarList.setProgressFlags(win32::ProgressFlag::Error);
                m_taskbarList.setProgressValue(1, 3);
                stopDuplication();
                awaitRetry();
                continue;
            }
            try {
                while (m_keepRunning && m_isDuplicationStarted && !m_hasError) {
                    render();
                    sleep();
                }
                rethrow();
            }
            catch (Expected &e) {
                OutputDebugStringA(e.text);
                OutputDebugStringA("\n");
                m_taskbarList.setProgressFlags(win32::ProgressFlag::Error);
                m_taskbarList.setProgressValue(2, 3);
                m_error = {};
                stopDuplication();
            }
        }
        stopDuplication();
        return m_returnValue;
    }

    void startDuplication() {
        if (m_isDuplicationStarted) return;
        m_isDuplicationStarted = true;
        try {
            auto deviceValue = renderer::createDevice();
            auto device = deviceValue.device;
            auto deviceContext = deviceValue.deviceContext;

            auto dimensions = renderer::getDimensionData(device, m_config.displays);

            m_target = renderer::createSharedTexture(device, rectSize(dimensions.rect));
            m_windowRenderer.init([&]() noexcept {
                auto args = WindowRenderer::InitArgs{};
                args.windowHandle = m_windowHandle;
                args.texture = m_target;
                args.device = device;
                args.deviceContext = deviceContext;
                return args;
            }());
            m_offset = POINT{dimensions.rect.left, dimensions.rect.top};
            auto handle = renderer::getSharedHandle(m_target);
            for (const auto &capture : m_captureThreads) {
                startCaptureThread(*capture, m_offset, handle);
            }
            resetFrameLatencyHandle();
        }
        catch (const renderer::Error &e) {
            throw Expected{e.message};
        }
    }

    void startCaptureThread(CaptureThread &capture, const POINT &offset, HANDLE targetHandle) {
        auto deviceValue = renderer::createDevice();
        auto device = deviceValue.device;
        auto deviceContext = deviceValue.deviceContext;

        FrameUpdater::InitArgs updater_args;
        updater_args.device = device;
        updater_args.deviceContext = deviceContext;
        updater_args.targetHandle = targetHandle;
        m_frameUpdaters.emplace_back(std::move(updater_args));

        CaptureThread::StartArgs threadArgs;
        threadArgs.device = device;
        threadArgs.offset = offset;
        m_threads.emplace_back(capture.start(std::move(threadArgs)));
    }

    void stopDuplication() {
        if (!m_isDuplicationStarted) return;
        try {
            for (const auto &capture : m_captureThreads) {
                capture->stop();
            }
            for (auto &thread : m_threads) {
                thread.join();
            }
            m_threads.clear();
            m_target.Reset();
            m_frameUpdaters.clear();
            m_updatedThreads.clear();
            m_windowRenderer.reset();
            resetFrameLatencyHandle();
        }
        catch (Expected &e) {
            OutputDebugStringA("stopDuplication Threw: ");
            OutputDebugStringA(e.text);
            OutputDebugStringA("\n");
        }
        m_isDuplicationStarted = false;
    }

    void render() {
        if (m_waitFrame || !m_doRender) return;
        m_doRender = false;
        ShowWindowBorder(m_windowHandle, !IsWindowMaximized(m_windowHandle));
        try {
            m_windowRenderer.render();
            m_windowRenderer.renderPointer(m_pointerUpdater.data());
            m_windowRenderer.swap();
            captureNextFrame();
            awaitNextFrame();
            updateVisibleArea();
        }
        catch (const renderer::Error &e) {
            throw Expected{e.message};
        }
    }

    void captureNextFrame() {
        if (m_isFreezed) return;
        for (auto index : m_updatedThreads) m_captureThreads[index]->next();
        m_updatedThreads.clear();
    }

    void awaitNextFrame() {
        if (!m_waitFrame) {
            m_waitFrame = true;

            if (m_frameLatencyIndex < 0) {
                m_frameLatencyIndex = m_handles.size();
                m_handles.push_back(m_windowRenderer.frameLatencyWaitable());
            }
        }
    }

    void awaitRetry() {
        if (m_canStartDuplication) {
            m_canStartDuplication = false;
            LARGE_INTEGER queueTime;
            queueTime.QuadPart = -250 * 1000 * 10; // relative 250 ms
            const auto noPeriod = 0;
            const auto apcArgument = this;
            const auto awakeSuspend = false;
            const auto success = SetWaitableTimer(
                m_retryTimer, &queueTime, noPeriod, retryDuplicationAPC, apcArgument, awakeSuspend);
            if (!success) throw Unexpected{"failed to arm retry timer"};
        }
    }

    void sleep(unsigned int timeout = INFINITE) {
        const auto wake_mask = QS_ALLINPUT;
        const auto flags = MWMO_ALERTABLE | MWMO_INPUTAVAILABLE;
        const auto handle_count = m_handles.size();
        const auto awoken = MsgWaitForMultipleObjectsEx(
            reinterpret_cast<const uint32_t &>(handle_count),
            m_handles.data(),
            timeout,
            wake_mask,
            flags);
        if (WAIT_FAILED == awoken) throw Unexpected{"Application Waiting failed"};
        if (m_frameLatencyIndex >= 0 && WAIT_OBJECT_0 + m_frameLatencyIndex == awoken) {
            m_waitFrame = false;
        }
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
                m_keepRunning = false;
                m_returnValue = reinterpret_cast<int &>(message.wParam);
            }
        }
    }

    void rethrow() {
        if (!m_hasError) return;
        m_hasError = false;
        std::rethrow_exception(m_error);
    }

    void updateError(const std::exception_ptr &exception) noexcept {
        m_error = exception;
        m_hasError = true;
    }
    void updateFrame(CapturedUpdate &update, const FrameContext &context, size_t threadIndex) {
        if (!m_isDuplicationStarted) return;
        m_frameUpdaters[threadIndex].update(update.frame, context);
        m_pointerUpdater.update(update.pointer, context);
        if (m_isFreezed) {
            m_updatedThreads.push_back(threadIndex);
        }
        else {
            m_captureThreads[threadIndex]->next();
        }
        m_doRender = true;
    }

private:
    Config m_config;
    DWORD m_processId{};
    HWND m_windowHandle{};
    UINT m_taskbarCreatedMessage{};
    win32::PowerRequest<PowerRequestDisplayRequired, PowerRequestSystemRequired> m_powerRequest;
    win32::TaskbarList m_taskbarList;
    HANDLE m_retryTimer{};
    int m_frameLatencyIndex{-1};

public:
    HANDLE m_threadHandle;

private:
    bool m_canStartDuplication = false;
    bool m_isDuplicationStarted = false;
    bool m_keepRunning = true;
    int m_returnValue = 0;
    bool m_hasError = false;
    std::exception_ptr m_error;
    bool m_waitFrame = false;
    bool m_doRender = false;

    POINT m_offset;
    WindowRenderer m_windowRenderer;
    ComPtr<ID3D11Texture2D> m_target;
    std::vector<FrameUpdater> m_frameUpdaters;
    PointerUpdater m_pointerUpdater;

    using capture_thread_ptr = std::unique_ptr<CaptureThread>;

    std::vector<HANDLE> m_handles;
    std::vector<std::thread> m_threads;
    std::vector<capture_thread_ptr> m_captureThreads;
    std::vector<size_t> m_updatedThreads;

    HWND m_visibleAreaWindow = nullptr;
};

} // namespace

struct Application::Impl : ::AppImpl {
    using ::AppImpl::AppImpl;
};

Application::Application(Config config)
#pragma warning(disable : 26409) // make_unique allows no custom deleter
    : m(new Impl(std::move(config))) {
}

int Application::run() { return m->run(); }

void CaptureThread::Callbacks::setError(std::exception_ptr Error) //
    [[gsl::suppress("26462")]] // type is not expressable
{
    auto self = reinterpret_cast<AppImpl *>(this);

    auto parameter = make_tuple_ptr(self, Error);
    const auto success = QueueUserAPC(
        AppImpl::setErrorAPC, self->m_threadHandle, ulong_ptr_cast(std::move(parameter)));
    if (!success) throw Unexpected{"api::setError failed to queue APC"};
}

void CaptureThread::Callbacks::setFrame(
    CapturedUpdate &&frame, const FrameContext &context, size_t thread_index) //
    [[gsl::suppress("26462")]] // type is not expressable
{
    auto self = reinterpret_cast<AppImpl *>(this);

    auto parameter = make_tuple_ptr(self, std::move(frame), std::ref(context), thread_index);
    const auto success = QueueUserAPC(
        AppImpl::setFrameAPC, self->m_threadHandle, ulong_ptr_cast(std::move(parameter)));
    if (!success) throw Unexpected{"api::setError failed to queue APC"};
}

void Application::ImplDeleter::operator()(Impl *ptr) noexcept { delete ptr; }
