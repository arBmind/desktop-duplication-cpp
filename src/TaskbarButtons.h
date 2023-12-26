#pragma once
#include "MainController.h"
#include "Model.h"

#include "win32/TaskbarList.h"
#include "win32/Window.h"

namespace deskdup {

using win32::Window;

/// manages the buttons in the Taskbar, when hovering
struct TaskbarButtons {
    TaskbarButtons();

    auto userMessage() const -> uint32_t { return m_taskbarCreatedMessage; }

    void createButtons(Window &, const State &state);

    bool handleCommand(int command, MainController &);

    void updateMaximized(bool isMaximized);
    void updateSystemStatus(SystemStatus);
    void updateDuplicationStatus(DuplicationStatus, PauseRendering);
    void updateVisibleArea(bool isShown);

private:
    void visualizeMaximized(bool isMaximized);
    void visualizeSystemStatus(SystemStatus);
    void visualizeDuplicationStatus(DuplicationStatus, PauseRendering);
    void visualizeVisibleArea(bool isShown);
    void finalizeButtons();

private:
    uint32_t m_taskbarCreatedMessage{};
    bool m_hasButtons{};
    win32::TaskbarList m_taskbarList{};

    bool m_updatedButtons{};
    bool m_showsMaximized{};
    SystemStatus m_showsSystemStatus{};
    DuplicationStatus m_showsDuplicationStatus{};
    bool m_showsVisibleArea{};
};

} // namespace deskdup
