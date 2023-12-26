#include "DisplayMonitor.h"

namespace win32 {

auto DisplayMonitor::fromPoint(Point point, DisplayMonitor::Fallback fallback) -> DisplayMonitor {
    auto flags = [&]() -> DWORD {
        switch (fallback) {
        case Fallback::Nearest: return MONITOR_DEFAULTTONEAREST;
        case Fallback::None: return MONITOR_DEFAULTTONULL;
        case Fallback::Primary: return MONITOR_DEFAULTTOPRIMARY;
        }
        return MONITOR_DEFAULTTONULL;
    }();
    return DisplayMonitor{::MonitorFromPoint(POINT{point.x, point.y}, flags)};
}

auto DisplayMonitor::fromRect(Rect rect, Fallback fallback) -> DisplayMonitor {
    auto flags = [&]() -> DWORD {
        switch (fallback) {
        case Fallback::Nearest: return MONITOR_DEFAULTTONEAREST;
        case Fallback::None: return MONITOR_DEFAULTTONULL;
        case Fallback::Primary: return MONITOR_DEFAULTTOPRIMARY;
        }
        return MONITOR_DEFAULTTONULL;
    }();
    auto winRect = rect.toRECT();
    return DisplayMonitor{::MonitorFromRect(&winRect, flags)};
}

auto DisplayMonitor::monitorInfo() const -> MonitorInfo {
    auto mi = MONITORINFO{};
    mi.cbSize = sizeof(MONITORINFO);
    ::GetMonitorInfoA(m_monitorHandle, &mi);
    return MonitorInfo{
        Rect::fromRECT(mi.rcMonitor),
        Rect::fromRECT(mi.rcWork),
        mi.dwFlags == MONITORINFOF_PRIMARY,
    };
}

} // namespace win32
