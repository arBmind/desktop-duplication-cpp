#include "TaskbarButtons.h"

namespace deskdup {

enum ThumbButton { Maximized, Freezed, VisibleArea };

TaskbarButtons::TaskbarButtons(Model &model)
    : m_model(model) {
    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarButtonCreated");

    ChangeWindowMessageFilter(m_taskbarCreatedMessage, MSGFLT_ADD);
    ChangeWindowMessageFilter(WM_COMMAND, MSGFLT_ADD);
}

void TaskbarButtons::createButtons(Window &window) {
    m_hasButtons = true;
    m_taskbarList = win32::TaskbarList{window.handle()};
    m_taskbarList.setButtonFlags(ThumbButton::Maximized, win32::ThumbButtonFlag::Enabled);
    m_taskbarList.setButtonTooltip(ThumbButton::Maximized, L"toggle fullscreen");
    visualizeMaximized();

    m_taskbarList.setButtonFlags(ThumbButton::Freezed, win32::ThumbButtonFlag::Enabled);
    m_taskbarList.setButtonTooltip(ThumbButton::Freezed, L"toggle freezed");
    visualizeDuplicationStatus();

    m_taskbarList.setButtonFlags(ThumbButton::VisibleArea, win32::ThumbButtonFlag::Enabled);
    m_taskbarList.setButtonTooltip(ThumbButton::VisibleArea, L"toggle area");
    visualizeVisibleArea();

    finalizeButtons();
}

bool TaskbarButtons::handleCommand(int command) {
    switch (command) {
    case ThumbButton::Maximized: return toggleMaximized(), true;
    case ThumbButton::Freezed: return toggleFreezed(), true;
    case ThumbButton::VisibleArea: return toggleVisibleArea(), true;
    }
    return false;
}

void TaskbarButtons::update() {
    if (!m_hasButtons) return;
    if (m_model.duplication().isMaximized() != m_showsMaximized) visualizeMaximized();
    if (m_model.duplication().status() != m_showsDuplicationStatus) visualizeDuplicationStatus();
    if (m_model.status() != m_showsSystemStatus) visualizeSystemStatus();
    if (m_model.visibleArea().isShown() != m_showsVisibleArea) visualizeVisibleArea();
    if (m_updatedButtons) finalizeButtons();
}

void TaskbarButtons::toggleMaximized() { m_model.duplication().toggleMaximized(); }

void TaskbarButtons::toggleFreezed() { m_model.duplication().toggleFreezed(); }

void TaskbarButtons::toggleVisibleArea() { m_model.visibleArea().toggleShown(); }

void TaskbarButtons::visualizeMaximized() {
    auto isMaximized = m_model.duplication().isMaximized();
    auto icon = isMaximized ? wchar_t{0xE923} : wchar_t{0xE922};
    m_taskbarList.setButtonLetterIcon(ThumbButton::Maximized, icon);
    m_showsMaximized = isMaximized;
    m_updatedButtons = true;
}

void TaskbarButtons::visualizeSystemStatus() {
    auto status = m_model.status();
    switch (status) {
    case SystemStatus::Neutral:
        m_taskbarList.setProgressFlags(win32::ProgressFlag::Disabled);
        break;
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
    m_showsSystemStatus = status;
}

void TaskbarButtons::visualizeDuplicationStatus() {
    auto status = m_model.duplication().status();
    switch (status) {
    case DuplicationStatus::Live:
        m_taskbarList.setButtonLetterIcon(1, 0xE103, RGB(255, 200, 10));
        break;

    case DuplicationStatus::Paused:
        m_taskbarList.setButtonLetterIcon(1, 0xE768, RGB(100, 255, 100));
        break;

    case DuplicationStatus::Blacked:
        m_taskbarList.setButtonLetterIcon(1, 0xE747, RGB(100, 100, 100));
        break;
    }
    m_showsDuplicationStatus = status;
    m_updatedButtons = true;
}

constexpr static auto colorKey = RGB(0xFF, 0x20, 0xFF);

void TaskbarButtons::visualizeVisibleArea() {
    auto isShown = m_model.visibleArea().isShown();
    auto color = isShown ? RGB(128, 128, 128) : colorKey;
    m_taskbarList.setButtonLetterIcon(ThumbButton::VisibleArea, 0xEF20, color);
    m_showsVisibleArea = isShown;
    m_updatedButtons = true;
}

void TaskbarButtons::finalizeButtons() {
    m_updatedButtons = false;
    m_taskbarList.updateThumbButtons();
}

} // namespace deskdup
