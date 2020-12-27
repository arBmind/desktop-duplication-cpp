#pragma once
#include "Model.h"

#include "win32/Window.h"

namespace deskdup {

using win32::MessageHandler;
using win32::Rect;
using win32::Window;
using win32::WindowClass;

/// top-most transparent window that marks the captured area of the screen
struct CaptureAreaWindow final : private MessageHandler<CaptureAreaWindow> {
    explicit CaptureAreaWindow(VisibleAreaModel &, const WindowClass &, const Window *parent);

    auto window() -> Window & { return m_window; }

    void update();

private:
    friend struct MessageHandler<CaptureAreaWindow>;
    bool paint() override;

private:
    VisibleAreaModel &m_model;
    win32::WindowWithMessages m_window;
    Rect m_shownAreaRect;
};

} // namespace deskdup
