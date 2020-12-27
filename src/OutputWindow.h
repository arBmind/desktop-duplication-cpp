#pragma once
#include "Model.h"
#include "TaskbarButtons.h"

#include "win32/Window.h"

namespace deskdup {

using win32::MessageHandler;
using win32::Optional;
using win32::OptLRESULT;
using win32::Window;
using win32::WindowClass;
using win32::WindowWithMessages;

/// Holder of the duplication output window
/// note:
/// * handles all messages
/// * reacts to model changes
struct OutputWindow final : MessageHandler<OutputWindow> {
    OutputWindow(Model &model, const WindowClass &windowClass);

    auto window() -> Window & { return m_window; }

    void update();

private:
    friend struct MessageHandler<OutputWindow>;
    void size(const Dimension &, uint32_t) override;
    void move(const Point &topLeft) override;

    void close() override;

    Optional<Point> m_draggingLastPosition{};
    void mouseMove(const Point &mousePosition, DWORD keyState) override;
    void mouseLeftButtonDown(const Point &mousePosition, DWORD keyState) override;
    void mouseLeftButtonUp(const Point &mousePosition, DWORD keyState) override;

    void mouseRightButtonUp(const Point &mousePosition, DWORD keyState) override;

    bool m_pickWindow{};
    void mouseRightDoubleClick(const Point &mousePosition, DWORD keyState) override;
    void killFocus() override;

    void inputKeyDown(uint32_t keyCode) override;

    void mouseLeftButtonDoubleClick(const Point &mousePosition, DWORD keyState) override;

    void mouseWheel(int wheelDelta, const Point &mousePosition, DWORD keyState) override;

    void dpiChanged(const Rect &rect) override;

    auto userMessage(const win32::WindowMessage &) -> OptLRESULT override;
    auto controlCommand(const win32::ControlCommand &) -> OptLRESULT override;

    auto createWindow(const WindowClass &) -> WindowWithMessages;

    void visualizeMaximized();
    void visualizeOutputRect();

private:
    DuplicationModel &m_duplicationModel;
    VisibleAreaModel &m_visibleAreaModel;
    WindowWithMessages m_window;
    TaskbarButtons m_taskbarButtons;

    bool m_showsMaximized{};
    Rect m_showsOutputRect{};
};

} // namespace deskdup
