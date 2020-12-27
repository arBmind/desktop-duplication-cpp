#pragma once
#include "Model.h"

#include "win32/TaskbarList.h"
#include "win32/Window.h"

namespace deskdup {

using win32::Window;

/// manages the buttons in the Taskbar, when hovering
struct TaskbarButtons {
    TaskbarButtons(Model &);

    auto userMessage() const -> uint32_t { return m_taskbarCreatedMessage; }

    void createButtons(Window &);

    bool handleCommand(int command);

    void update();

private:
    void toggleMaximized();
    void toggleFreezed();
    void toggleVisibleArea();

    void visualizeMaximized();
    void visualizeSystemStatus();
    void visualizeDuplicationStatus();
    void visualizeVisibleArea();
    void finalizeButtons();

private:
    Model &m_model;
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
