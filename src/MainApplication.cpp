#include "MainApplication.h"

#include "Model.h"
#include "meta/scope_guard.h"

#include <algorithm>
#include <bit>

namespace deskdup {

using win32::Dimension;
using win32::DisplayMonitor;
using win32::WindowClass;

namespace {

static auto windowWorkArea() noexcept -> Rect {
    auto rect = RECT{};
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
    return Rect::fromRECT(rect);
}

auto createWindowClass(HINSTANCE instanceHandle) -> WindowClass::Config {
    const auto cursor = LoadCursor(nullptr, IDC_CROSS);
    if (!cursor) throw Unexpected{"LoadCursor failed"};
    LATER(DestroyCursor(cursor));

    const auto icon = LoadIconW(instanceHandle, L"desk1");

    auto config = WindowClass::Config{};
    config.instanceHandle = instanceHandle;
    config.className = L"deskdupl";
    config.icon = win32::Icon{icon};
    config.smallIcon = win32::Icon{icon};
    config.cursor = win32::Cursor{cursor};
    return config;
}

auto defaultConfig() -> Config {
    auto config = Config{};
    config.isCaptureAreaShown = true;
    const auto workArea = windowWorkArea();
    config.outputTopLeft = Point{workArea.topLeft.x + workArea.dimension.width / 2, workArea.topLeft.y + 32};
    config.outputDimension = Dimension{workArea.dimension.width / 2, workArea.dimension.height / 2};
    return config;
}

} // namespace

MainApplication::MainApplication(HINSTANCE instanceHandle)
    : m_state{
          .config = defaultConfig(),
      }
    , m_mainThread{createWindowClass(instanceHandle)} {

    m_state.duplicationStatus =
        m_state.config.operationMode == OperationMode::CaptureArea ? DuplicationStatus::Pause : DuplicationStatus::Live;

    refreshMonitors();
}

int MainApplication::run() {
    try {
        m_powerRequest = PowerRequest{L"Desktop Duplication Tool"};

        m_outputWindow.emplace(OutputWindow::Args{
            .windowClass = m_mainThread.windowClass(),
            .mainController = *this,
        });
        m_captureAreaWindow.emplace(CaptureAreaWindow::Args{
            .windowClass = m_mainThread.windowClass(),
            .rect = operatonModeLens().captureAreaRect(),
        });
        m_duplicationController.emplace(DuplicationController::Args{
            .windowClass = m_mainThread.windowClass(),
            .mainController = *this,
            .mainThread = m_mainThread.thread(),
            .outputWindow = m_outputWindow->window(),
        });

        m_outputWindow->show(m_state.outputMaximized);
        if (m_captureAreaWindow) {
            m_captureAreaWindow->updateIsShown(m_state.config.isCaptureAreaShown);
            m_captureAreaWindow->updateDuplicationStatus(m_state.duplicationStatus);
        }
        m_outputWindow->updateDuplicationStatus(m_state.duplicationStatus, m_state.config.pauseRendering);
        if (m_duplicationController) m_duplicationController->updateDuplicationStatus(m_state.duplicationStatus);

        return m_mainThread.run();
    }
    catch (const Unexpected &e) {
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
        return -1;
    }
}

void MainApplication::quit() { PostQuitMessage(0); }

void MainApplication::changeOperationMode(OperationMode operationMode) {
    if (m_state.config.operationMode != operationMode) {
        m_state.config.operationMode = operationMode;
        if (m_outputWindow) m_outputWindow->updateOperationMode(operationMode);
        if (operationMode == OperationMode::CaptureArea) {
            if (m_captureAreaWindow) m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
            if (m_duplicationController) {
                m_duplicationController->updateCaptureOffset(operatonModeLens().captureOffset());
                m_duplicationController->updateOutputZoom(operatonModeLens().outputZoom());
            }
            m_state.outputMonitor = m_state.config.captureMonitor;
            updateCaptureAreaOutputScreen();
        }
        if (operationMode == OperationMode::PresentMirror) {
            if (m_captureAreaWindow) m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
            if (m_duplicationController) {
                m_duplicationController->updateCaptureOffset(operatonModeLens().captureOffset());
                m_duplicationController->updateOutputZoom(operatonModeLens().outputZoom());
            }
            if (m_state.outputMonitor != m_state.config.captureMonitor) {
                if (m_state.duplicationStatus == DuplicationStatus::Live && m_duplicationController)
                    m_duplicationController->restart();
                updateScreenRect(m_state.monitors[m_state.config.captureMonitor].info.monitorRect);
            }
        }
    }
}

void MainApplication::updateSystemStatus(SystemStatus systemStatus) {
    if (m_state.systemStatus != systemStatus) {
        m_state.systemStatus = systemStatus;
        if (m_outputWindow) m_outputWindow->taskbarButtons().updateSystemStatus(systemStatus);
    }
}

void MainApplication::updateScreenRect(Rect rect) {
    if (m_state.captureMonitorRect != rect) {
        m_state.captureMonitorRect = rect;
        if (config().operationMode == OperationMode::PresentMirror && m_captureAreaWindow)
            m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
        if (config().operationMode == OperationMode::CaptureArea && m_duplicationController)
            m_duplicationController->updateCaptureOffset(operatonModeLens().captureOffset());
    }
}

void MainApplication::captureScreen(int screen) {
    if (config().operationMode != OperationMode::PresentMirror) return; // not used in this mode
    if (m_state.config.captureMonitor != screen && m_state.monitors.size() > screen) {
        m_state.config.captureMonitor = screen;
        if (m_state.duplicationStatus == DuplicationStatus::Live && m_duplicationController)
            m_duplicationController->restart();
        updateScreenRect(m_state.monitors[m_state.config.captureMonitor].info.monitorRect);
    }
}

void MainApplication::updateOutputRect(Rect rect) {
    auto dimensionChanged = m_state.config.outputDimension != rect.dimension;
    auto topLeftChanged = m_state.config.outputTopLeft != rect.topLeft;
    if (dimensionChanged || topLeftChanged) {
        m_state.config.outputDimension = rect.dimension;
        m_state.config.outputTopLeft = rect.topLeft;
        if (m_outputWindow) m_outputWindow->updateRect(m_state.config.outputRect());
        if (config().operationMode == OperationMode::PresentMirror) {
            if (dimensionChanged && m_captureAreaWindow)
                m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
        }
        if (dimensionChanged && m_duplicationController)
            m_duplicationController->updateOutputDimension(m_state.config.outputDimension);
        if (config().operationMode == OperationMode::CaptureArea) {
            if (m_captureAreaWindow) m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
            if (topLeftChanged && m_duplicationController)
                m_duplicationController->updateCaptureOffset(operatonModeLens().captureOffset());

            updateCaptureAreaOutputScreen();
        }
    }
}

void MainApplication::resizeOutputWindow(Dimension dimension, bool isMaximized) {
    if (m_state.config.outputDimension != dimension) {
        m_state.config.outputDimension = dimension;
        if (m_outputWindow) m_outputWindow->updateRect(m_state.config.outputRect());
        if (m_captureAreaWindow) m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
        if (m_duplicationController) m_duplicationController->updateOutputDimension(m_state.config.outputDimension);
    }
    if (m_state.outputMaximized != isMaximized) {
        m_state.outputMaximized = isMaximized;
        if (m_outputWindow) m_outputWindow->updateMaximized(m_state.outputMaximized);
        // updatePowerRequest
        const auto success = isMaximized ? m_powerRequest.set() : m_powerRequest.clear();
        if (!success) {
            OutputDebugStringA("Power Request Failed!\n");
        }
    }
}

void MainApplication::moveOutputWindowTo(Point topLeft) {
    if (topLeft != m_state.config.outputTopLeft) {
        m_state.config.outputTopLeft = topLeft;
        // note: on maximize the is triggered wrongly
        // if (m_outputWindow) m_outputWindow->updateRect(m_state.config.outputRect());
        if (config().operationMode == OperationMode::CaptureArea) {
            if (m_captureAreaWindow) m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
            if (m_duplicationController)
                m_duplicationController->updateCaptureOffset(operatonModeLens().captureOffset());

            updateCaptureAreaOutputScreen();
        }
    }
}

void MainApplication::moveCaptureOffsetByScreen(int dx, int dy) {
    if (config().operationMode != OperationMode::PresentMirror) return;
    if (dx != 0 || dy != 0) {
        m_state.config.captureOffset.x += static_cast<float>(dx) / m_state.config.outputZoom;
        m_state.config.captureOffset.y += static_cast<float>(dy) / m_state.config.outputZoom;
        m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
        if (m_duplicationController) m_duplicationController->updateCaptureOffset(operatonModeLens().captureOffset());
    }
}

void MainApplication::resetOutputZoom() {
    if (config().operationMode != OperationMode::PresentMirror) return; // not used in this mode
    if (m_state.config.outputZoom != 1.0f) {
        m_state.config.outputZoom = 1.0f;
        m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
        if (m_duplicationController) m_duplicationController->updateOutputZoom(operatonModeLens().outputZoom());
    }
}

void MainApplication::zoomOutputBy(float delta) {
    if (config().operationMode != OperationMode::PresentMirror) return; // not used in this mode
    if (delta >= 0.0001f || delta <= 0.0001f) {
        m_state.config.outputZoom += delta;
        m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
        if (m_duplicationController) m_duplicationController->updateOutputZoom(operatonModeLens().outputZoom());
    }
}

void MainApplication::zoomOutputAt(Point point, float delta) {
    if (config().operationMode != OperationMode::PresentMirror) return; // not used in this mode
    if (delta >= 0.0001f || delta <= 0.0001f) {
        auto mx = point.x - m_state.config.outputTopLeft.x;
        auto my = point.y - m_state.config.outputTopLeft.y;
        auto offsetPoint = Vec2f{
            static_cast<float>(mx / m_state.config.outputZoom),
            static_cast<float>(my / m_state.config.outputZoom),
        };
        m_state.config.outputZoom += delta;
        m_state.config.captureOffset.x -= offsetPoint.x - static_cast<float>(mx) / m_state.config.outputZoom;
        m_state.config.captureOffset.y -= offsetPoint.y - static_cast<float>(my) / m_state.config.outputZoom;
        m_captureAreaWindow->updateRect(operatonModeLens().captureAreaRect());
        if (m_duplicationController) {
            m_duplicationController->updateCaptureOffset(operatonModeLens().captureOffset());
            m_duplicationController->updateOutputZoom(operatonModeLens().outputZoom());
        }
    }
}

void MainApplication::toggleVisualizeCaptureArea() {
    auto isShown = (m_state.config.isCaptureAreaShown = !m_state.config.isCaptureAreaShown);
    m_outputWindow->taskbarButtons().updateVisibleArea(isShown);
    m_captureAreaWindow->updateIsShown(isShown);
}

void MainApplication::changeDuplicationStatus(DuplicationStatus status) {
    if (m_state.duplicationStatus != status) {
        m_state.duplicationStatus = status;
        if (m_outputWindow)
            m_outputWindow->updateDuplicationStatus(m_state.duplicationStatus, m_state.config.pauseRendering);
        if (m_captureAreaWindow) m_captureAreaWindow->updateDuplicationStatus(m_state.duplicationStatus);
        if (m_duplicationController) m_duplicationController->updateDuplicationStatus(m_state.duplicationStatus);
    }
}

void MainApplication::toggleOutputMaximize() {
    m_outputWindow->show(!m_state.outputMaximized); // note: this triggers a maximized event
}

void MainApplication::refreshMonitors() {
    resetMonitors();
    if (m_state.config.operationMode == OperationMode::PresentMirror) {
        if (m_state.config.captureMonitor >= m_state.monitors.size()) {
            captureScreen(0);
        }
        else {
            updateScreenRect(m_state.monitors[m_state.config.captureMonitor].info.monitorRect);
        }
    }
    auto outputSample = Point{
        m_state.config.outputTopLeft.x + m_state.config.outputDimension.width / 2, m_state.config.outputTopLeft.y};
    auto isOutputVisible = std::any_of(m_state.monitors.begin(), m_state.monitors.end(), [&](Monitor const &m) {
        return m.info.monitorRect.contains(outputSample);
    });
    if (!isOutputVisible) {
        auto workArea = m_state.monitors[0].info.workRect;
        updateOutputRect(Rect{
            Point{workArea.topLeft.x + workArea.dimension.width / 2, workArea.topLeft.y + 32},
            Dimension{workArea.dimension.width / 2, workArea.dimension.height / 2}});
    }
}

bool MainApplication::updateCaptureAreaOutputScreen() {
    auto dm = DisplayMonitor::fromRect(m_state.config.outputRect());
    if (dm.handle() != m_state.monitors[m_state.outputMonitor].handle) {
        m_state.outputMonitor = m_state.computeIndexForMonitorHandle(dm.handle());
        updateScreenRect(m_state.monitors[m_state.outputMonitor].info.monitorRect);
        if (m_duplicationController) {
            m_duplicationController->restart();
        }
        return true;
    }
    return false;
}

void MainApplication::resetMonitors() {
    auto monitors = Monitors{};
    DisplayMonitor::enumerateAll([&](DisplayMonitor dm, Rect) {
        monitors.push_back({
            .handle = dm.handle(),
            .info = dm.monitorInfo(),
        });
    });
    if (monitors.empty()) {
        OutputDebugStringA("No Monitors found!");
        std::exit(9);
    }
    if (monitors != m_state.monitors) {
        m_state.monitors = monitors;
    }
}

} // namespace deskdup
