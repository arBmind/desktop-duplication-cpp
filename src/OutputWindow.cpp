#include "OutputWindow.h"
#include "Model.h"

namespace deskdup {

namespace {

void ShowWindowBorder(HWND windowHandle, bool shown) noexcept {
    auto style = GetWindowLong(windowHandle, GWL_STYLE);
    auto exstyle = GetWindowLong(windowHandle, GWL_EXSTYLE);
    const auto flags = WS_THICKFRAME | WS_CAPTION;
    if (shown) {
        style |= flags;
        exstyle |= WS_EX_OVERLAPPEDWINDOW;
    }
    else {
        style &= ~flags;
        exstyle &= ~WS_EX_OVERLAPPEDWINDOW;
    }
    SetWindowLong(windowHandle, GWL_STYLE, style);
    SetWindowLong(windowHandle, GWL_EXSTYLE, exstyle);
}

auto createWindow(win32::WindowClass const &windowClass, Rect rect) -> win32::WindowWithMessages {
    return windowClass.createWindow({
        .style = win32::WindowStyle::overlappedWindow(),
        .name = L"Duplicate Desktop Presenter (Double Click to toggle fullscreen)",
        .rect = rect,
    });
}

auto windowRegion(HWND windowHandle) {
    auto windowInfo = WINDOWINFO{};
    windowInfo.cbSize = sizeof(WINDOWINFO);
    ::GetWindowInfo(windowHandle, &windowInfo);
    auto top = (windowInfo.rcClient.top - windowInfo.rcWindow.top);
    auto left = (windowInfo.rcClient.left - windowInfo.rcWindow.left);
    auto right = (windowInfo.rcWindow.right - windowInfo.rcClient.right);
    auto bottom = (windowInfo.rcWindow.bottom - windowInfo.rcClient.bottom);
    auto w = windowInfo.rcWindow.right - windowInfo.rcWindow.left;
    auto h = windowInfo.rcWindow.bottom - windowInfo.rcWindow.top;

    auto polys = std::array{
        // outer
        POINT{0, 0},
        POINT{w, 0},
        POINT{w, h},
        POINT{0, h},
        // inner
        POINT{left, top},
        POINT{left, h - bottom},
        POINT{w - right, h - bottom},
        POINT{w - right, top},
    };
    auto edgeCounts = std::array{4, 4};
    auto rgn = CreatePolyPolygonRgn(polys.data(), edgeCounts.data(), static_cast<int>(edgeCounts.size()), WINDING);
    auto redraw = BOOL{TRUE};
    SetWindowRgn(windowHandle, rgn, redraw);
    DeleteObject(rgn);
}

} // namespace

OutputWindow::OutputWindow(Args const &initArgs)
    : m_controller{initArgs.mainController}
    , m_window{createWindow(initArgs.windowClass, m_controller.config().outputRect())} {
    m_window.setMessageHandler(this);
}

void OutputWindow::show(bool isMaximized) {
    updateMaximized(isMaximized);
    if (isMaximized) {
        m_window.showMaximized();
    }
    else {
        m_window.showNormal();
    }
}

void OutputWindow::updateRect(Rect rect) {
    m_window.moveClient(rect);
    if (m_controller.config().operationMode == OperationMode::CaptureArea &&
        m_controller.state().duplicationStatus == DuplicationStatus::Pause) {
        windowRegion(m_window.handle());
    }
}

void OutputWindow::updateMaximized(bool isMaximized) {
    ShowWindowBorder(m_window.handle(), !isMaximized);
    m_taskbarButtons.updateMaximized(isMaximized);
}

void OutputWindow::updateOperationMode(OperationMode operationMode) {
    if (operationMode == OperationMode::CaptureArea) {
        if (m_controller.state().duplicationStatus == DuplicationStatus::Live) {
            ::SetForegroundWindow(::GetNextWindow(m_window.handle(), GW_HWNDNEXT));
            m_window.toBottom();
        }
        else {
            windowRegion(m_window.handle());
        }
    }
    if (operationMode == OperationMode::PresentMirror) {
        if (m_controller.state().duplicationStatus == DuplicationStatus::Live) {
            m_window.toTop();
        }
        else {
            auto redraw = BOOL{TRUE};
            SetWindowRgn(m_window.handle(), nullptr, redraw);
        }
    }
}

void OutputWindow::updateDuplicationStatus(DuplicationStatus status, PauseRendering pauseRendering) {
    if (m_controller.config().operationMode == OperationMode::CaptureArea) {
        if (status == DuplicationStatus::Live) {
            auto redraw = BOOL{TRUE};
            SetWindowRgn(m_window.handle(), nullptr, redraw);
            ::SetForegroundWindow(::GetNextWindow(m_window.handle(), GW_HWNDNEXT));
            m_window.toBottom();
        }
        else {
            m_window.toTop();
            windowRegion(m_window.handle());
        }
    }
    m_taskbarButtons.updateDuplicationStatus(status, pauseRendering);
}

void OutputWindow::size(const Dimension &dimension, uint32_t flags) {
    if (flags == SIZE_MINIMIZED) return;
    m_controller.resizeOutputWindow(dimension, (flags == SIZE_MAXIMIZED));
    if (m_controller.config().operationMode == OperationMode::CaptureArea &&
        m_controller.state().duplicationStatus == DuplicationStatus::Pause) {
        windowRegion(m_window.handle());
    }
}

void OutputWindow::move(const Point &topLeft) { m_controller.moveOutputWindowTo(topLeft); }

bool OutputWindow::position(const win32::WindowPosition &windowPosition) {
    if (windowPosition.hasZOrder()) {
        auto topWindow = ::GetTopWindow(nullptr);
        if ((windowPosition.insertAfter == topWindow || windowPosition.insertAfter == HWND_NOTOPMOST) &&
            m_controller.config().operationMode == OperationMode::CaptureArea) {
            m_controller.changeDuplicationStatus(DuplicationStatus::Pause);
        }
    }
    return false;
}

void OutputWindow::close() { m_controller.quit(); }

void OutputWindow::mouseMove(const Point &mousePosition, DWORD keyState) {
    if (keyState == (MK_LBUTTON | MK_SHIFT)) {
        if (m_draggingLastPosition) {
            auto dx = mousePosition.x - m_draggingLastPosition->x;
            auto dy = mousePosition.y - m_draggingLastPosition->y;
            m_controller.moveCaptureOffsetByScreen(dx, dy);
            m_draggingLastPosition = mousePosition;
        }
    }
}

void OutputWindow::mouseLeftButtonDown(const Point &mousePosition, DWORD keyState) {
    if (keyState == (MK_LBUTTON | MK_SHIFT)) {
        m_draggingLastPosition = mousePosition;
    }
}

void OutputWindow::mouseLeftButtonUp(const Point & /*mousePosition*/, DWORD /*keyState*/) {
    m_draggingLastPosition = {};
}

bool OutputWindow::mouseRightButtonUp(const Point & /*mousePosition*/, DWORD keyState) {
    if (keyState == MK_SHIFT) {
        m_controller.toggleVisualizeCaptureArea();
        return true;
    }
    else if (keyState == MK_CONTROL) {
        m_pickWindow ^= true;
        return true;
    }
    return false;
}

enum MenuCommands : int {
    Menu_TogglePause = 1,
    Menu_Resolutions = 100,
    Menu_ResolutionMax = 199,
    Menu_Screens = 200,
    Menu_ScreenMax = 299,
    Menu_ModePresentMirror = 300,
    Menu_ModeCaptureRegion = 301,
};
struct Resolution {
    win32::Dimension dim;
    win32::Name label;
};

auto resolutions = std::array{
    Resolution{{3840, 2800}, L"3840x2800"},
    Resolution{{3840, 2560}, L"3840x2560"},
    Resolution{{2560, 1600}, L"2560x1600"},
    Resolution{{2560, 1440}, L"2560x1440"},
    Resolution{{1920, 1200}, L"1920x1200"},
    Resolution{{1920, 1080}, L"1920x1080"},
    Resolution{{1280, 960}, L"1280x960"},
    Resolution{{1280, 720}, L"1280x720"},
    Resolution{{1024, 768}, L"1024x768"},
};

void OutputWindow::contextMenu(const Point &position) {
    auto hPopupMenu = CreatePopupMenu();
    AppendMenu(
        hPopupMenu,
        MF_STRING,
        Menu_TogglePause,
        m_controller.state().duplicationStatus == DuplicationStatus::Pause ? L"Play" : L"Pause");
    AppendMenu(hPopupMenu, MF_SEPARATOR, 0, nullptr);

    auto hResolutionMenu = CreatePopupMenu();
    auto &cfg = m_controller.config();
    for (auto i = 0u; auto &res : resolutions) {
        auto isChecked = (res.dim == cfg.outputDimension);
        AppendMenu(hResolutionMenu, isChecked ? MF_CHECKED : MF_STRING, Menu_Resolutions + i, res.label.data());
        ++i;
    }
    AppendMenu(hPopupMenu, MF_POPUP, std::bit_cast<UINT_PTR>(hResolutionMenu), L"Resolutions");

    auto hScreenMenu = CreatePopupMenu();
    auto currentMonitor = m_controller.operatonModeLens().captureMonitor();
    for (auto scr = 0; scr < 2; ++scr) {
        auto label = std::wstring{L"0"};
        label[0] = static_cast<wchar_t>(label[0] + scr);
        auto flags = UINT{MF_STRING};
        if (currentMonitor == scr) {
            flags |= MF_CHECKED;
        }
        if (cfg.operationMode != OperationMode::PresentMirror) {
            flags |= MF_DISABLED;
        }
        AppendMenu(hScreenMenu, flags, Menu_Screens + scr, label.data());
    }
    AppendMenu(hPopupMenu, MF_POPUP, std::bit_cast<UINT_PTR>(hScreenMenu), L"Capture Screen");

    AppendMenu(hPopupMenu, MF_SEPARATOR, 0, nullptr);
    {
        auto flags =
            m_controller.config().operationMode == OperationMode::PresentMirror ? UINT{MF_CHECKED} : UINT{MF_STRING};
        AppendMenu(hPopupMenu, flags, Menu_ModePresentMirror, L"Present Mirror");
    }
    {
        auto flags =
            m_controller.config().operationMode == OperationMode::CaptureArea ? UINT{MF_CHECKED} : UINT{MF_STRING};
        AppendMenu(hPopupMenu, flags, Menu_ModeCaptureRegion, L"Capture Region");
    }
    auto const menuPos = [&]() {
        if (position.x < 0 || position.y < 0) {
            auto tmp = POINT{};
            ClientToScreen(m_window.handle(), &tmp);
            return Point::fromPOINT(tmp);
        }
        return position;
    }();
    TrackPopupMenuEx(hPopupMenu, 0, menuPos.x, menuPos.y, m_window.handle(), nullptr);
    DestroyMenu(hResolutionMenu);
    DestroyMenu(hScreenMenu);
    DestroyMenu(hPopupMenu);
}

auto OutputWindow::menuCommand(uint16_t command) -> win32::OptLRESULT {
    if (command == Menu_TogglePause) m_controller.togglePause();
    if (command >= Menu_Resolutions && command <= Menu_ResolutionMax) {
        auto &res = resolutions.at(command - Menu_Resolutions);
        m_controller.resizeOutputWindow(res.dim, false);
        return LRESULT{};
    }
    if (command >= Menu_Screens && command <= Menu_ScreenMax) {
        auto scr = command - Menu_Screens;
        m_controller.captureScreen(scr);
        return LRESULT{};
    }
    if (command == Menu_ModePresentMirror) {
        m_controller.changeOperationMode(OperationMode::PresentMirror);
    }
    if (command == Menu_ModeCaptureRegion) {
        m_controller.changeOperationMode(OperationMode::CaptureArea);
    }
    return {};
}

void OutputWindow::mouseRightDoubleClick(const Point & /*mousePosition*/, DWORD /*keyState*/) { m_pickWindow ^= true; }

void OutputWindow::setFocus() {
    if (m_controller.config().operationMode == OperationMode::CaptureArea) {
        m_controller.changeDuplicationStatus(DuplicationStatus::Pause);
    }
}

void OutputWindow::killFocus() {
    if (m_pickWindow) {
        m_pickWindow = false;
        auto window = win32::Window::fromForeground();
        auto rect = m_controller.operatonModeLens().captureAreaRect();
        auto mk = modifierKeys();
        if (mk == MK_SHIFT)
            window.moveBorder(rect);
        else
            window.moveClient(rect);
    }
}

bool OutputWindow::inputKeyDown(uint32_t keyCode) {
    const auto mk = modifierKeys();
    switch (keyCode) {
    case VK_UP:
        if (mk == MK_SHIFT) m_controller.moveCaptureOffsetByScreen(0, -1);
        return true;
    case VK_DOWN:
        if (mk == MK_SHIFT) m_controller.moveCaptureOffsetByScreen(0, +1);
        return true;
    case VK_LEFT:
        if (mk == MK_SHIFT) m_controller.moveCaptureOffsetByScreen(-1, 0);
        return true;
    case VK_RIGHT:
        if (mk == MK_SHIFT) m_controller.moveCaptureOffsetByScreen(+1, 0);
        return true;
    case VK_RETURN:
        if (mk == MK_CONTROL) m_controller.toggleOutputMaximize();
        return true;
    default: {
        const auto ch = ::MapVirtualKey(keyCode, MAPVK_VK_TO_CHAR);
        switch (ch) {
        case '0':
            if (mk & MK_CONTROL) m_controller.resetOutputZoom();
            return true;
        case '+':
            if (mk == MK_CONTROL) {
                m_controller.zoomOutputBy(0.01f);
            }
            else if (mk & MK_CONTROL) {
                m_controller.zoomOutputBy(0.001f);
            }
            return true;
        case '-':
            if (mk == MK_CONTROL) {
                m_controller.zoomOutputBy(-0.01f);
            }
            else if (mk & MK_CONTROL) {
                m_controller.zoomOutputBy(-0.001f);
            }
            return true;
        case 'F': // Freeze
        case 'P': // Pause / Play
            if (mk == MK_CONTROL) {
                m_controller.togglePause();
            }
            return true;
        }
    }
    }
    return false;
}

void OutputWindow::mouseLeftButtonDoubleClick(const win32::Point & /*mousePosition*/, DWORD /*keyState*/) {
    m_controller.toggleOutputMaximize();
}

void OutputWindow::mouseWheel(int wheelDelta, const Point &mousePosition, DWORD keyState) {
    if (0 != (keyState & MK_CONTROL)) {
        auto change =
            static_cast<float>(wheelDelta) / ((0 != (keyState & MK_SHIFT)) ? 300 * WHEEL_DELTA : 30 * WHEEL_DELTA);
        m_controller.zoomOutputAt(mousePosition, change);
    }
}

void OutputWindow::displayChange(uint32_t /*bitsPerPixel*/, Dimension) { m_controller.refreshMonitors(); }

void OutputWindow::dpiChanged(const win32::Rect &rect) { m_window.move(rect); }

auto OutputWindow::userMessage(const win32::WindowMessage &message) -> OptLRESULT {
    if (message.type == m_taskbarButtons.userMessage()) {
        m_taskbarButtons.createButtons(m_window, m_controller.state());
        return 0;
    }
    return {};
}

auto OutputWindow::controlCommand(const win32::ControlCommand &command) -> OptLRESULT {
    if (m_taskbarButtons.handleCommand(command.identifier, m_controller)) return 0;
    return {};
}

} // namespace deskdup
