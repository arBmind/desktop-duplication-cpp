#include "CaptureAreaWindow.h"

namespace deskdup {
namespace {

using win32::WindowWithMessages;

constexpr static auto colorKey = RGB(0xFF, 0x20, 0xFF);

auto borderRect(const Rect &rect) -> Rect {
    return Rect{
        Point{rect.left() - 2, rect.top() - 2},
        Dimension{rect.width() + 4, rect.height() + 4},
    };
}

auto createWindow(const Rect &rect, const WindowClass &windowClass, const Window *parent)
    -> WindowWithMessages {

    auto config = WindowWithMessages::Config{};
    config.style = win32::WindowStyle::topmostOverlay();
    // config.style = win32::WindowStyle::overlappedWindow();
    config.rect = borderRect(rect);
    config.parent = parent;
    return windowClass.createWindow(config);
}

void makeTransparent(Window &window) {
    const BYTE alpha = 0u;
    const BYTE flags = LWA_COLORKEY;
    ::SetLayeredWindowAttributes(window.handle(), colorKey, alpha, flags);
}

} // namespace

CaptureAreaWindow::CaptureAreaWindow(
    VisibleAreaModel &model, const win32::WindowClass &windowClass, const win32::Window *parent)
    : m_model(model)
    , m_window(createWindow(m_model.rect(), windowClass, parent))
    , m_shownAreaRect(m_model.rect()) {
    m_window.setMessageHandler(this);
    makeTransparent(m_window);
    m_window.show();
    m_window.update();
}

void CaptureAreaWindow::update() {
    if (m_model.rect() != m_shownAreaRect) {
        m_shownAreaRect = m_model.rect();
        m_window.move(borderRect(m_shownAreaRect));
    }
}

bool CaptureAreaWindow::paint() {
    const auto dim = m_shownAreaRect.dimension;

    auto p = PAINTSTRUCT{};
    auto dc = BeginPaint(m_window.handle(), &p);

    SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    SelectObject(dc, GetStockObject(DC_PEN));

    SetDCPenColor(dc, RGB(255, 80, 40));
    Rectangle(dc, 0, 0, dim.width + 4, dim.height + 4);

    SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, colorKey);
    SetDCPenColor(dc, RGB(255, 255, 255));
    Rectangle(dc, 1, 1, dim.width + 3, dim.height + 3);

    EndPaint(m_window.handle(), &p);
    return true;
}

} // namespace deskdup
