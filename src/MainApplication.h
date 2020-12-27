#pragma once
#include "CaptureAreaWindow.h"
#include "DuplicationController.h"
#include "Model.h"
#include "OutputWindow.h"

#include "win32/PowerRequest.h"
#include "win32/ThreadLoop.h"

namespace deskdup {

struct Application {
    Application(Model &model, HINSTANCE instanceHandle);

    static auto defaultOutputRect() -> Rect;

    int run();

private:
    void update();

private:
    win32::WindowClass m_windowClass;
    Model &m_model;
    Optional<OutputWindow> m_outputWindow{};
    Optional<DuplicationController> m_duplicationController{};
    Optional<CaptureAreaWindow> m_captureAreaWindow{};

    using PowerRequest =
        win32::PowerRequest<PowerRequestDisplayRequired, PowerRequestSystemRequired>;
    PowerRequest m_powerRequest;
    bool m_hasPowerRequest{};
};

} // namespace deskdup
