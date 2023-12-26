#pragma once
#include "CaptureAreaWindow.h"
#include "DuplicationController.h"
#include "MainController.h"
#include "MainThread.h"
#include "Model.h"
#include "OutputWindow.h"

#include "win32/PowerRequest.h"

namespace deskdup {

struct MainApplication final : private MainController {
    explicit MainApplication(HINSTANCE instanceHandle);

    int run();

    // MainController interface
private:
    auto state() const -> State const & override { return m_state; }

    void quit() override;
    void changeOperationMode(OperationMode) override;
    void updateSystemStatus(SystemStatus) override;
    void updateScreenRect(Rect) override;
    void captureScreen(int) override;
    void resizeOutputWindow(Dimension, bool isMaximized) override;
    void moveOutputWindowTo(Point) override;
    void moveCaptureOffsetByScreen(int dx, int dy) override;
    void resetOutputZoom() override;
    void zoomOutputBy(float) override;
    void zoomOutputAt(Point, float) override;
    void toggleVisualizeCaptureArea() override;
    void changeDuplicationStatus(DuplicationStatus) override;
    void toggleOutputMaximize() override;
    void refreshMonitors() override;

private:
    bool updateCaptureAreaOutputScreen();
    void updateOutputRect(Rect);
    void resetMonitors();

private:
    State m_state{};
    MainThread m_mainThread;

    Optional<OutputWindow> m_outputWindow{};
    Optional<CaptureAreaWindow> m_captureAreaWindow{};
    Optional<DuplicationController> m_duplicationController{};

    using PowerRequest = win32::PowerRequest<PowerRequestDisplayRequired, PowerRequestSystemRequired>;
    PowerRequest m_powerRequest;
    bool m_hasPowerRequest{};
};

} // namespace deskdup
