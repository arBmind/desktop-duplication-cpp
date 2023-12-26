#include "Window.h"

// #include <dwmapi.h>

namespace win32 {

namespace {

auto extractWindow(HWND windowHandle, UINT message, LPARAM lParam) -> WindowWithMessages * {
    switch (message) {
    case WM_NCCREATE: {
        auto create_struct = std::bit_cast<LPCREATESTRUCT>(lParam);
        auto *window = static_cast<WindowWithMessages *>(create_struct->lpCreateParams);
        ::SetWindowLongPtrW(windowHandle, GWLP_USERDATA, std::bit_cast<LONG_PTR>(window));
        // windowClass->handleWindowSetup(window);
        return window;
    }
    default: return std::bit_cast<WindowWithMessages *>(::GetWindowLongPtr(windowHandle, GWLP_USERDATA));
    }
}

auto ensureInstance(WindowClass::Config &config) {
    if (!config.instanceHandle) {
        config.instanceHandle = ::GetModuleHandleW(nullptr);
    }
    return config.instanceHandle;
}

auto createWindow(void *window, const WindowWithMessages::Config &config, HINSTANCE instance, const Name &className)
    -> HWND {
    return ::CreateWindowExW(
        config.style.exStyle,
        className.data(),
        config.name.data(),
        config.style.style,
        config.rect.topLeft.x,
        config.rect.topLeft.y,
        config.rect.dimension.width,
        config.rect.dimension.height,
        config.parent ? config.parent->handle() : HWND{},
        config.menu.hMenu,
        instance,
        window);
}

} // namespace

//
// Window
//

auto Window::rect() const -> Rect {
    auto rect = RECT{};
    ::GetWindowRect(m_windowHandle, &rect);
    return Rect::fromRECT(rect);
}

// auto Window::extendedFrameRect() const -> Rect {
//     auto rect = RECT{};
//     ::DwmGetWindowAttribute(m_windowHandle, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
//     return Rect::fromRECT(rect);
// }

auto Window::clientRect() const -> Rect {
    auto rect = RECT{};
    ::GetClientRect(m_windowHandle, &rect);
    return Rect::fromRECT(rect);
}

auto Window::clientOffset() const -> Point {
    auto windowInfo = WINDOWINFO{};
    windowInfo.cbSize = sizeof(WINDOWINFO);
    ::GetWindowInfo(m_windowHandle, &windowInfo);
    return Point{
        .x = windowInfo.rcWindow.left - windowInfo.rcClient.left,
        .y = windowInfo.rcWindow.top - windowInfo.rcClient.top,
    };
}

auto Window::placement() const -> Placement {
    auto windowPlacement = WINDOWPLACEMENT{};
    windowPlacement.length = sizeof(WINDOWPLACEMENT);
    ::GetWindowPlacement(m_windowHandle, &windowPlacement);
    auto currentRect = rect();
    return Placement{
        [&]() -> ShowState {
            if (windowPlacement.showCmd == SW_SHOWMAXIMIZED) return ShowState::Maximized;
            if (windowPlacement.showCmd == SW_SHOWMINIMIZED) return ShowState::Minimized;
            return ShowState::Normal;
        }(),
        // Rect::fromPOINTS(windowPlacement.ptMinPosition, windowPlacement.ptMaxPosition),
        currentRect, // use correct values!
        Rect::fromRECT(windowPlacement.rcNormalPosition),
    };
}

bool Window::isMaximized() const { return ::IsZoomed(m_windowHandle); }
bool Window::isMinimized() const { return ::IsIconic(m_windowHandle); }

auto Window::style() const -> WindowStyle {
    auto style = (DWORD)::GetWindowLongPtr(m_windowHandle, GWL_STYLE);
    auto exStyle = (DWORD)::GetWindowLongPtr(m_windowHandle, GWL_EXSTYLE);
    return WindowStyle{style, exStyle};
}

bool Window::isVisible() const { return ::IsWindowVisible(m_windowHandle); }

auto Window::dpiAwareness() const -> DpiAwareness {
    auto context = ::GetWindowDpiAwarenessContext(m_windowHandle);
    if (context == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) return DpiAwareness::PerMonitorAwareV2;
    if (context == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) return DpiAwareness::PerMonitorAware;
    if (context == DPI_AWARENESS_CONTEXT_SYSTEM_AWARE) return DpiAwareness::SystemAware;
    if (context == DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED) return DpiAwareness::UnawareGdiScaled;
    if (context == DPI_AWARENESS_CONTEXT_UNAWARE) return DpiAwareness::Unaware;
    return DpiAwareness::Unknown;
}

auto Window::dpi() const -> Dpi { return Dpi{::GetDpiForWindow(m_windowHandle)}; }

auto Window::text() const -> String {
    auto result = String{};
    auto len = ::GetWindowTextLengthW(m_windowHandle);
    if (len) {
        auto maxSize = len + 2;
        result.resize(maxSize);
        auto actual = ::GetWindowTextW(m_windowHandle, result.data(), maxSize);
        result.resize(actual);
        result.shrink_to_fit();
    }
    return result;
}

// note: not working on Windows10 (only sees own module)
// auto Window::moduleFileName() const -> String {
//     auto result = String{};
//     result.resize(65535);
//     auto actual = ::GetWindowModuleFileNameW(m_windowHandle, result.data(), result.size());
//     result.resize(actual);
//     result.shrink_to_fit();
//     return result;
// }

void Window::show() { ::ShowWindowAsync(m_windowHandle, SW_SHOW); }
void Window::hide() { ::ShowWindowAsync(m_windowHandle, SW_HIDE); }

void Window::toBottom() {
    auto flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE;
    ::SetWindowPos(m_windowHandle, HWND_BOTTOM, 0, 0, 0, 0, flags);
}

void Window::toTop() {
    auto flags = SWP_NOMOVE | SWP_NOSIZE;
    ::SetWindowPos(m_windowHandle, ::GetTopWindow(nullptr), 0, 0, 0, 0, flags);
    ::SetForegroundWindow(m_windowHandle);
}

void Window::showNormal() { ::ShowWindowAsync(m_windowHandle, SW_SHOWNORMAL); }
void Window::showMaximized() { ::ShowWindowAsync(m_windowHandle, SW_SHOWMAXIMIZED); }
void Window::showMinimized() { ::ShowWindowAsync(m_windowHandle, SW_SHOWMINIMIZED); }

void Window::update() { ::UpdateWindow(m_windowHandle); }

void Window::move(const Rect &rect) {
    const auto repaint = true;
    ::MoveWindow(m_windowHandle, rect.left(), rect.top(), rect.width(), rect.height(), repaint);
}
void Window::moveBorder(const Rect &rect) {
    auto wRect = rect.toRECT();
    auto hasMenu = false;
    ::AdjustWindowRectEx(&wRect, WS_THICKFRAME, hasMenu, WS_EX_OVERLAPPEDWINDOW);
    auto aRect = Rect::fromRECT(wRect);
    auto top = rect.top();
    auto height = aRect.height() - (top - aRect.top());
    const auto repaint = true;
    ::MoveWindow(m_windowHandle, aRect.left(), top, aRect.width(), height, repaint);
}

void Window::moveClient(const Rect &rect) {
    auto windowInfo = WINDOWINFO{};
    windowInfo.cbSize = sizeof(WINDOWINFO);
    ::GetWindowInfo(m_windowHandle, &windowInfo);
    auto top = rect.top() - windowInfo.rcClient.top + windowInfo.rcWindow.top;
    auto left = rect.left() - windowInfo.rcClient.left + windowInfo.rcWindow.left;
    auto right = rect.right() - windowInfo.rcClient.right + windowInfo.rcWindow.right;
    auto bottom = rect.bottom() - windowInfo.rcClient.bottom + windowInfo.rcWindow.bottom;
    const auto repaint = true;
    ::MoveWindow(m_windowHandle, left, top, right - left, bottom - top, repaint);
}

void Window::setPosition(const Point &point) {
    auto flags = SWP_NOZORDER | SWP_NOSIZE | SWP_NOSENDCHANGING | SWP_ASYNCWINDOWPOS;
    ::SetWindowPos(m_windowHandle, nullptr, point.x, point.y, 0, 0, flags);
}

bool Window::setPlacement(const Placement &placement) {
    auto windowPlacement = WINDOWPLACEMENT{};
    windowPlacement.length = sizeof(WINDOWPLACEMENT);
    windowPlacement.flags = WPF_ASYNCWINDOWPLACEMENT;
    windowPlacement.showCmd = [&]() -> UINT {
        switch (placement.showState) {
        case ShowState::Normal: return SW_SHOWNORMAL;
        case ShowState::Maximized: return SW_SHOWMAXIMIZED;
        case ShowState::Minimized: return SW_SHOWMINIMIZED;
        }
        return SW_HIDE;
    }();
    windowPlacement.ptMinPosition = placement.currentRect.topLeft.toPOINT();
    windowPlacement.ptMaxPosition = placement.currentRect.bottomRight().toPOINT();
    windowPlacement.rcNormalPosition = placement.normalRect.toRECT();
    return 0 != ::SetWindowPlacement(m_windowHandle, &windowPlacement);
}

void Window::styleNonLayered() {
    auto exStyle = GetWindowLong(m_windowHandle, GWL_EXSTYLE) & ~WS_EX_LAYERED;
    SetWindowLong(m_windowHandle, GWL_EXSTYLE, exStyle);
}

void Window::styleLayeredColorKey(uint32_t color) {
    auto exStyle = GetWindowLong(m_windowHandle, GWL_EXSTYLE) | WS_EX_LAYERED;
    SetWindowLong(m_windowHandle, GWL_EXSTYLE, exStyle);
    const auto alpha = BYTE{0u};
    SetLayeredWindowAttributes(m_windowHandle, color, alpha, LWA_COLORKEY);
}

void Window::styleLayeredAlpha(uint8_t alpha) {
    auto exStyle = GetWindowLong(m_windowHandle, GWL_EXSTYLE) | WS_EX_LAYERED;
    SetWindowLong(m_windowHandle, GWL_EXSTYLE, exStyle);
    const auto color = DWORD{0u};
    SetLayeredWindowAttributes(m_windowHandle, color, alpha, LWA_ALPHA);
}

//
// WindowWithMessages
//

WindowWithMessages::WindowWithMessages(
    const WindowWithMessages::Config &config, HINSTANCE instance, const Name &className) {
    m_windowHandle = createWindow(this, config, instance, className);
}

WindowWithMessages::~WindowWithMessages() noexcept { ::DestroyWindow(m_windowHandle); }

void WindowWithMessages::resetMessageHandler() {
    m_messageHandlerFunc = &WindowWithMessages::noopMessageHandler;
    m_messageHandlerPtr = nullptr;
}

auto WindowWithMessages::noopMessageHandler(void *, const WindowMessage &) -> OptLRESULT { return {}; }

auto WindowWithMessages::handleMessage(const WindowMessage &msg) -> OptLRESULT {
    return m_messageHandlerFunc(m_messageHandlerPtr, msg);
}

//
// WindowClass
//

WindowClass::WindowClass(Config config)
    : m_instanceHandle(ensureInstance(config))
    , m_className(config.className)
    , m_atom(registerWindowClass(config)) {}

WindowClass::~WindowClass() noexcept { ::UnregisterClassW(m_className.data(), m_instanceHandle); }

auto WindowClass::createWindow(WindowWithMessages::Config config) const -> WindowWithMessages {
    return WindowWithMessages{config, m_instanceHandle, m_className};
}

void WindowClass::recreateWindow(WindowWithMessages &wnd, Window::Config config) const {
    wnd.~WindowWithMessages();
    new (&wnd) WindowWithMessages{config, m_instanceHandle, m_className};
}

auto WindowClass::registerWindowClass(const Config &config) -> ATOM {
    auto windowClass = WNDCLASSEXW{};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = WindowClass::staticWindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = config.instanceHandle;
    windowClass.hIcon = config.icon.hIcon;
    windowClass.hCursor = config.cursor.hCursor;
    windowClass.hbrBackground = nullptr;
    windowClass.lpszMenuName = config.menuName.data(); // TODO: ensure zero!
    windowClass.lpszClassName = config.className.data();
    windowClass.hIconSm = config.smallIcon.hIcon;

    return RegisterClassExW(&windowClass);
}

auto CALLBACK WindowClass::staticWindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {

    auto *window = extractWindow(windowHandle, message, lParam);
    if (window) {
        auto result = window->handleMessage(WindowMessage{windowHandle, message, wParam, lParam});
        if (result) return *result;
    }
    return ::DefWindowProc(windowHandle, message, wParam, lParam);
}

} // namespace win32
