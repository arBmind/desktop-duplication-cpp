#include "CaptureAreaWindow.h"

namespace deskdup {
namespace {

using win32::WindowWithMessages;

constexpr static auto colorKey = RGB(0xFF, 0x20, 0xFF);

auto borderRect(Rect rect) -> Rect {
    return Rect{
        Point{rect.left() - 2, rect.top() - 2},
        Dimension{rect.width() + 4, rect.height() + 4},
    };
}

auto createWindow(const WindowClass &windowClass, Rect rect) -> WindowWithMessages {
    return windowClass.createWindow({
        .style = win32::WindowStyle::popup().topMost().transparent(),
        .name = win32::Name{},
        .rect = borderRect(rect),
    });
}

} // namespace

CaptureAreaWindow::CaptureAreaWindow(const Args &args)
    : m_window{createWindow(args.windowClass, args.rect)}
    , m_dimension{args.rect.dimension} {
    m_window.setMessageHandler(this);
    m_window.styleLayeredColorKey(colorKey);
}

void CaptureAreaWindow::updateIsShown(bool isShown) {
    if (isShown) {
        m_window.show();
        m_window.update();
    }
    else {
        m_window.hide();
    }
}

void CaptureAreaWindow::updateDuplicationStatus(DuplicationStatus status) {
    m_duplicationStatus = status;
    auto rect = m_window.rect();
    rect.dimension.width += 1;
    m_window.move(rect);
    rect.dimension.width -= 1;
    m_window.move(rect);
}

void CaptureAreaWindow::updateRect(Rect rect) {
    m_dimension = rect.dimension;
    m_window.move(borderRect(rect));
}

bool CaptureAreaWindow::paint() {
    const auto dim = m_dimension;

    auto p = PAINTSTRUCT{};
    auto dc = BeginPaint(m_window.handle(), &p);

    SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    SelectObject(dc, GetStockObject(DC_PEN));

    SetDCPenColor(dc, m_duplicationStatus == DuplicationStatus::Live ? RGB(255, 80, 40) : RGB(255, 220, 120));
    Rectangle(dc, 0, 0, dim.width + 4, dim.height + 4);

    SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, colorKey);
    SetDCPenColor(dc, RGB(255, 255, 255));
    Rectangle(dc, 1, 1, dim.width + 3, dim.height + 3);

    EndPaint(m_window.handle(), &p);
    return true;
}

} // namespace deskdup
