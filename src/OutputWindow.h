#pragma once
#include "MainController.h"
#include "Model.h"
#include "TaskbarButtons.h"

#include "win32/Window.h"

#include <optional>

namespace deskdup {

using win32::OptLRESULT;
using win32::Window;
using win32::WindowClass;
using win32::WindowMessageHandler;
using win32::WindowWithMessages;

/// Holder of the duplication output window
/// note:
/// * handles all messages
/// * reacts to model changes
struct OutputWindow final : private WindowMessageHandler<OutputWindow> {
    struct Args {
        WindowClass const &windowClass;
        MainController &mainController;
    };
    OutputWindow(Args const &);

    auto window() -> WindowWithMessages & { return m_window; }
    auto taskbarButtons() -> TaskbarButtons & { return m_taskbarButtons; }

    // used after creation
    void show(bool isMaximized);

    void updateRect(Rect);
    void updateMaximized(bool isMaximized);
    void updateOperationMode(OperationMode);
    void updateDuplicationStatus(DuplicationStatus, PauseRendering);

private:
    friend struct WindowMessageHandler<OutputWindow>;
    void size(const Dimension &, uint32_t) override;
    void move(const Point &topLeft) override;
    bool position(const win32::WindowPosition &) override;

    void close() override;

    std::optional<Point> m_draggingLastPosition{};
    void mouseMove(const Point &mousePosition, DWORD keyState) override;
    void mouseLeftButtonDown(const Point &mousePosition, DWORD keyState) override;
    void mouseLeftButtonUp(const Point &mousePosition, DWORD keyState) override;

    bool mouseRightButtonUp(const Point &mousePosition, DWORD keyState) override;

    void contextMenu(const Point &position) override;
    auto menuCommand(uint16_t command) -> OptLRESULT override;

    bool m_pickWindow{};
    void mouseRightDoubleClick(const Point &mousePosition, DWORD keyState) override;
    void setFocus() override;
    void killFocus() override;

    bool inputKeyDown(uint32_t keyCode) override;

    void mouseLeftButtonDoubleClick(const Point &mousePosition, DWORD keyState) override;

    void mouseWheel(int wheelDelta, const Point &mousePosition, DWORD keyState) override;

    void displayChange(uint32_t bitsPerPixel, Dimension) override;
    void dpiChanged(const Rect &rect) override;

    auto userMessage(const win32::WindowMessage &) -> OptLRESULT override;
    auto controlCommand(const win32::ControlCommand &) -> OptLRESULT override;

private:
    MainController &m_controller;
    WindowWithMessages m_window;
    TaskbarButtons m_taskbarButtons;
};

} // namespace deskdup
