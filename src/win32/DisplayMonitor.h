#pragma once
#include "Geometry.h"

#include <Windows.h>

namespace win32 {

struct MonitorInfo {
    Rect monitorRect{};
    Rect workRect{};
    bool isPrimary{};
};

/// Wrapper around a display/monitor in Windows
struct DisplayMonitor {
    enum Fallback {
        Nearest,
        None,
        Primary,
    };

    explicit DisplayMonitor() = default;
    explicit DisplayMonitor(HMONITOR hMonitor)
        : m_monitorHandle(hMonitor) {}

    /// construct a DisplayMonitor for a given virtual screen point
    static auto fromPoint(const Point &point, Fallback fallback = Fallback::Nearest)
        -> DisplayMonitor;

    /// fall given f for each DisplayMonitor in the system
    template<class F>
    requires requires(F f, DisplayMonitor dm) {
        f(dm, Rect{});
    }
    static void enumerateAll(F &&f) {
        struct Callback {
            static auto CALLBACK
            enumerateCallback(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM lParam) -> BOOL {
                auto *fp = reinterpret_cast<F *>(lParam);
                (*fp)(DisplayMonitor{hMonitor}, Rect::fromRECT(*rect));
                return true;
            }
        };
        ::EnumDisplayMonitors(
            nullptr, nullptr, &Callback::enumerateCallback, reinterpret_cast<LPARAM>(&f));
    }

    /// true if handle is valid
    bool isValid() const { return m_monitorHandle != HMONITOR{}; }

    /// handle to call other win32 APIs
    auto handle() const -> HMONITOR { return m_monitorHandle; }

    /// retrieves MonitorInfo
    auto monitorInfo() const -> MonitorInfo;

private:
    HMONITOR m_monitorHandle{};
};

} // namespace win32
