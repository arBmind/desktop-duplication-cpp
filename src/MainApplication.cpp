#include "MainApplication.h"

#include "meta/scope_guard.h"

namespace deskdup {

using win32::Dimension;

namespace {

static auto windowWorkArea() noexcept -> Rect {
    auto rect = RECT{};
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
    return Rect::fromRECT(rect);
}

auto createWindowClass(HINSTANCE instanceHandle) -> win32::WindowClass {
    const auto cursor = LoadCursor(nullptr, IDC_CROSS);
    if (!cursor) throw Unexpected{"LoadCursor failed"};
    LATER(DestroyCursor(cursor));

    const auto icon = LoadIconW(instanceHandle, L"desk1");

    auto config = win32::WindowClass::Config{};
    config.instanceHandle = instanceHandle;
    config.className = L"deskdupl";
    config.icon = win32::Icon{icon};
    config.smallIcon = win32::Icon{icon};
    config.cursor = win32::Cursor{cursor};
    return win32::WindowClass{config};
}

} // namespace

Application::Application(Model &model, HINSTANCE instanceHandle)
    : m_windowClass(createWindowClass(instanceHandle))
    , m_model(model) {}

auto Application::defaultOutputRect() -> Rect {
    const auto workArea = windowWorkArea();
    return Rect{
        Point{workArea.topLeft.x + workArea.dimension.width / 2, workArea.topLeft.y},
        Dimension{workArea.dimension.width / 2, workArea.dimension.height / 2},
    };
}

int Application::run() {
    try {
        auto mainLoop = win32::ThreadLoop{};
        mainLoop.enableAwaitAlerts();

        m_powerRequest = PowerRequest{TEXT("Desktop Duplication Tool")};

        m_outputWindow.emplace(m_model, m_windowClass);
        m_duplicationController.emplace(m_model, mainLoop, m_outputWindow->window());
        m_model.capture().setStatus(CaptureStatus::Enabling);

        while (mainLoop.keepRunning()) {
            update();
            mainLoop.sleep();
        }
        return mainLoop.returnValue();
    }
    catch (const Unexpected &e) {
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
        return -1;
    }
}

void Application::update() {
    m_model.updateStatus();
    m_outputWindow->update();
    m_duplicationController->update();
    if (m_model.visibleArea().isShown() && !m_captureAreaWindow) {
        m_captureAreaWindow.emplace(
            m_model.visibleArea(), m_windowClass, &m_outputWindow->window());
    }
    if (!m_model.visibleArea().isShown() && m_captureAreaWindow) {
        m_captureAreaWindow.reset();
    }
    if (m_captureAreaWindow) {
        m_model.updateVisibleAreaRect();
        m_captureAreaWindow->update();
    }
    if (m_hasPowerRequest != m_model.duplication().isMaximized()) {
        m_hasPowerRequest = m_model.duplication().isMaximized();
        const auto success = m_hasPowerRequest ? m_powerRequest.set() : m_powerRequest.clear();
        if (!success) {
            OutputDebugStringA("Power Request Failed!\n");
        }
    }
}

} // namespace deskdup
