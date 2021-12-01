#include "OutputWindow.h"

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

} // namespace

OutputWindow::OutputWindow(Model &model, const win32::WindowClass &windowClass)
    : m_duplicationModel(model.duplication())
    , m_visibleAreaModel(model.visibleArea())
    , m_window(createWindow(windowClass))
    , m_taskbarButtons(model) {
    m_window.setMessageHandler(this);
    m_window.show();
}

void OutputWindow::update() {
    m_taskbarButtons.update();
    if (m_showsMaximized != m_duplicationModel.isMaximized()) {
        visualizeMaximized();
    }
    if (m_showsOutputRect != m_duplicationModel.outputRect()) {
        visualizeOutputRect();
    }
}

void OutputWindow::size(const Dimension &dimension, uint32_t flags) {
    m_showsMaximized = (flags == SIZE_MAXIMIZED);
    m_showsOutputRect.dimension = dimension;
    m_duplicationModel.setIsMaximized(m_showsMaximized);
    m_duplicationModel.setOutputDimension(dimension);
}

void OutputWindow::move(const Point &topLeft) {
    m_showsOutputRect.topLeft = topLeft;
    m_duplicationModel.setOutputTopLeft(topLeft);
}

void OutputWindow::close() { PostQuitMessage(0); }

void OutputWindow::mouseMove(const Point &mousePosition, DWORD keyState) {
    if (keyState == (MK_LBUTTON | MK_SHIFT)) {
        if (m_draggingLastPosition) {
            auto dx = mousePosition.x - m_draggingLastPosition->x;
            auto dy = mousePosition.y - m_draggingLastPosition->y;
            m_duplicationModel.moveCaptureOffsetByScreen(dx, dy);
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

void OutputWindow::mouseRightButtonUp(const Point & /*mousePosition*/, DWORD keyState) {
    if (0 != (keyState & MK_SHIFT)) {
        m_visibleAreaModel.toggleShown();
    }
}

void OutputWindow::mouseRightDoubleClick(const Point & /*mousePosition*/, DWORD /*keyState*/) {
    m_pickWindow ^= true;
}

void OutputWindow::killFocus() {
    if (m_pickWindow) {
        m_pickWindow = false;
        auto window = win32::Window::fromForeground();
        auto rect = m_visibleAreaModel.rect();
        auto mk = modifierKeys();
        if (mk == MK_SHIFT)
            window.moveBorder(rect);
        else
            window.moveClient(rect);
    }
}

void OutputWindow::inputKeyDown(uint32_t keyCode) {
    const auto mk = modifierKeys();
    switch (keyCode) {
    case VK_UP:
        if (mk == MK_SHIFT) m_duplicationModel.moveCaptureOffsetByScreen(0, -1);
        break;
    case VK_DOWN:
        if (mk == MK_SHIFT) m_duplicationModel.moveCaptureOffsetByScreen(0, +1);
        break;
    case VK_LEFT:
        if (mk == MK_SHIFT) m_duplicationModel.moveCaptureOffsetByScreen(-1, 0);
        break;
    case VK_RIGHT:
        if (mk == MK_SHIFT) m_duplicationModel.moveCaptureOffsetByScreen(+1, 0);
        break;
    default: {
        const auto ch = ::MapVirtualKey(keyCode, MAPVK_VK_TO_CHAR);
        switch (ch) {
        case '0':
            if (mk & MK_CONTROL) m_duplicationModel.resetZoom();
            break;
        case '+':
            if (mk == MK_CONTROL) {
                m_duplicationModel.zoomBy(0.01f);
            }
            else if (mk & MK_CONTROL) {
                m_duplicationModel.zoomBy(0.001f);
            }
            break;
        case '-':
            if (mk == MK_CONTROL) {
                m_duplicationModel.zoomBy(-0.01f);
            }
            else if (mk & MK_CONTROL) {
                m_duplicationModel.zoomBy(-0.001f);
            }
            break;
        case 'F': // Freeze
        case 'P': // Pause / Play
            if (mk == MK_CONTROL) {
                m_duplicationModel.toggleFreezed();
            }
        }
    }
    }
}

void OutputWindow::mouseLeftButtonDoubleClick(
    const win32::Point & /*mousePosition*/, DWORD /*keyState*/) {
    m_duplicationModel.setIsMaximized(!m_duplicationModel.isMaximized());
}

void OutputWindow::mouseWheel(int wheelDelta, const Point &mousePosition, DWORD keyState) {
    if (0 != (keyState & MK_CONTROL)) {
        auto change = static_cast<float>(wheelDelta) /
                      ((0 != (keyState & MK_SHIFT)) ? 300 * WHEEL_DELTA : 30 * WHEEL_DELTA);
        m_duplicationModel.zoomAtScreen(mousePosition, change);
    }
}

void OutputWindow::dpiChanged(const win32::Rect &rect) { m_window.move(rect); }

auto OutputWindow::userMessage(const win32::WindowMessage &message) -> OptLRESULT {
    if (message.type == m_taskbarButtons.userMessage()) {
        m_taskbarButtons.createButtons(m_window);
        return 0;
    }
    return {};
}

auto OutputWindow::controlCommand(const win32::ControlCommand &command) -> OptLRESULT {
    if (m_taskbarButtons.handleCommand(command.identifier)) return 0;
    return {};
}

auto OutputWindow::createWindow(const win32::WindowClass &windowClass)
    -> win32::WindowWithMessages {

    auto config = win32::WindowWithMessages::Config{};
    config.style = win32::WindowStyle::overlappedWindow();
    config.name = L"Duplicate Desktop Presenter (Double Click to toggle fullscreen)";
    config.rect = m_duplicationModel.outputRect();
    return windowClass.createWindow(config);
}

void OutputWindow::visualizeMaximized() {
    auto isMaximized = m_duplicationModel.isMaximized();
    ShowWindowBorder(m_window.handle(), !isMaximized);
    if (isMaximized) {
        m_window.showMaximized();
    }
    else {
        m_window.showNormal();
    }
    // m_showsMaximized = isMaximized; // updated by window event!
}

void OutputWindow::visualizeOutputRect() {
    auto rect = m_duplicationModel.outputRect();
    m_window.move(rect);
    // m_showsOutputRect = rect; // updated by window event!
}

} // namespace deskdup
