#pragma once
#include "Model.h"

#include "win32/Window.h"

namespace deskdup {

using win32::WindowMessageHandler;
using win32::Rect;
using win32::Window;
using win32::WindowClass;

/// top-most transparent window that marks the captured area of the screen
struct CaptureAreaWindow final : private WindowMessageHandler<CaptureAreaWindow> {
    struct Args {
        WindowClass const &windowClass;
        Rect rect{};
    };
    explicit CaptureAreaWindow(Args const &);

    auto window() -> Window & { return m_window; }

    void updateIsShown(bool isShown);
    void updateDuplicationStatus(DuplicationStatus);
    void updateRect(Rect);

private:
    friend struct WindowMessageHandler<CaptureAreaWindow>;
    bool paint() override;

private:
    win32::WindowWithMessages m_window;
    Dimension m_dimension;
    DuplicationStatus m_duplicationStatus;
};

} // namespace deskdup
