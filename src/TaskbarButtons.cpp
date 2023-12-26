#include "TaskbarButtons.h"

namespace deskdup {

enum ThumbButton { Maximized, Freezed, VisibleArea };

TaskbarButtons::TaskbarButtons() {
    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarButtonCreated");

    ChangeWindowMessageFilter(m_taskbarCreatedMessage, MSGFLT_ADD);
    ChangeWindowMessageFilter(WM_COMMAND, MSGFLT_ADD);
}

void TaskbarButtons::createButtons(Window &window, State const &state) {
    m_hasButtons = true;
    m_taskbarList = win32::TaskbarList{window.handle()};
    m_taskbarList.setButtonFlags(ThumbButton::Maximized, win32::ThumbButtonFlag::Enabled);
    m_taskbarList.setButtonTooltip(ThumbButton::Maximized, L"toggle fullscreen");
    visualizeMaximized(state.outputMaximized);

    m_taskbarList.setButtonFlags(ThumbButton::Freezed, win32::ThumbButtonFlag::Enabled);
    m_taskbarList.setButtonTooltip(ThumbButton::Freezed, L"toggle freezed");
    visualizeDuplicationStatus(state.duplicationStatus, state.config.pauseRendering);

    m_taskbarList.setButtonFlags(ThumbButton::VisibleArea, win32::ThumbButtonFlag::Enabled);
    m_taskbarList.setButtonTooltip(ThumbButton::VisibleArea, L"toggle area");
    visualizeVisibleArea(state.config.isCaptureAreaShown);

    finalizeButtons();
}

bool TaskbarButtons::handleCommand(int command, MainController &controller) {
    switch (command) {
    case ThumbButton::Maximized: return controller.toggleOutputMaximize(), true;
    case ThumbButton::Freezed: return controller.togglePause(), true;
    case ThumbButton::VisibleArea: return controller.toggleVisualizeCaptureArea(), true;
    }
    return false;
}

void TaskbarButtons::updateMaximized(bool isMaximized) {
    if (!m_hasButtons) return;
    visualizeMaximized(isMaximized);
    finalizeButtons();
}

void TaskbarButtons::updateSystemStatus(SystemStatus status) {
    if (!m_hasButtons) return;
    visualizeSystemStatus(status);
}

void TaskbarButtons::updateDuplicationStatus(DuplicationStatus status, PauseRendering pauseRendering) {
    if (!m_hasButtons) return;
    visualizeDuplicationStatus(status, pauseRendering);
    finalizeButtons();
}

void TaskbarButtons::updateVisibleArea(bool isShown) {
    if (!m_hasButtons) return;
    visualizeVisibleArea(isShown);
    finalizeButtons();
}

void TaskbarButtons::visualizeMaximized(bool isMaximized) {
    auto icon = isMaximized ? wchar_t{0xE923} : wchar_t{0xE922};
    m_taskbarList.setButtonLetterIcon(ThumbButton::Maximized, icon);
    m_updatedButtons = true;
}

void TaskbarButtons::visualizeSystemStatus(SystemStatus status) {
    switch (status) {
    case SystemStatus::Neutral: m_taskbarList.setProgressFlags(win32::ProgressFlag::Disabled); break;
    case SystemStatus::Green:
        m_taskbarList.setProgressFlags(win32::ProgressFlag::Normal);
        m_taskbarList.setProgressValue(1, 1);
        break;
    case SystemStatus::Yellow:
        m_taskbarList.setProgressFlags(win32::ProgressFlag::Paused);
        m_taskbarList.setProgressValue(1, 1);
        break;
    case SystemStatus::Red:
        m_taskbarList.setProgressFlags(win32::ProgressFlag::Error);
        m_taskbarList.setProgressValue(1, 3);
        break;
    }
}

void TaskbarButtons::visualizeDuplicationStatus(DuplicationStatus status, PauseRendering pauseRendering) {
    if (status == DuplicationStatus::Live) {
        m_taskbarList.setButtonLetterIcon(1, 0xE103, RGB(255, 200, 10));
    }
    else {
        using enum PauseRendering;
        switch (pauseRendering) {
        case LastFrame: m_taskbarList.setButtonLetterIcon(1, 0xE768, RGB(100, 255, 100)); break;
        case Black: m_taskbarList.setButtonLetterIcon(1, 0xE747, RGB(100, 100, 100)); break;
        case White: m_taskbarList.setButtonLetterIcon(1, 0xE747, RGB(255, 255, 255)); break;
        }
    }
    m_updatedButtons = true;
}

constexpr static auto colorKey = RGB(0xFF, 0x20, 0xFF);

void TaskbarButtons::visualizeVisibleArea(bool isShown) {
    auto color = isShown ? RGB(128, 128, 128) : colorKey;
    m_taskbarList.setButtonLetterIcon(ThumbButton::VisibleArea, 0xEF20, color);
    m_updatedButtons = true;
}

void TaskbarButtons::finalizeButtons() {
    m_updatedButtons = false;
    m_taskbarList.updateThumbButtons();
}

} // namespace deskdup
