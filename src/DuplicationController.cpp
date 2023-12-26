#include "DuplicationController.h"
#include "CapturedUpdate.h"
#include "FrameUpdater.h"
#include "Model.h"
#include "renderer.h"

#include <chrono>

namespace deskdup {
namespace {

auto createRetryTimer() -> WaitableTimer {
    auto config = WaitableTimer::Config{};
    config.timerName = TEXT("Local:RetryTimer");
    return WaitableTimer{config};
}

auto createWindowConfig(const Window &parent, Dimension dim) -> win32::WindowWithMessages::Config {
    return {
        .style = win32::WindowStyle::child(),
        .name = win32::Name{L"Hello"},
        .rect = Rect{.topLeft = {}, .dimension = dim},
        .parent = &parent,
    };
}

} // namespace

DuplicationController::DuplicationController(const Args &args)
    : m_windowClass{args.windowClass}
    , m_controller{args.mainController}
    , m_mainThread{args.mainThread}
    , m_outputWindow{args.outputWindow}
    , m_renderWindow{m_windowClass.createWindow(
          createWindowConfig(m_outputWindow, m_controller.config().outputRect().dimension))}
    , m_renderThread{renderThreadConfig(m_controller.operatonModeLens())}
    , m_captureThread{captureThreadConfig()}
    , m_retryTimer{createRetryTimer()} {
    m_renderWindow.setCustomHandler<&DuplicationController::forwardRenderMessage>(this);
    m_renderThread.start();
}

DuplicationController::~DuplicationController() { resetOnMain(); }

void DuplicationController::updateDuplicationStatus(DuplicationStatus status) {
    using enum DuplicationStatus;
    switch (status) {
    case Live: return startOnMain();
    case Pause: return pauseOnMain();
    }
}

void DuplicationController::updateOutputDimension(Dimension dimension) {
    m_renderWindow.moveClient(Rect{.topLeft = {}, .dimension = dimension});
    m_renderThread.thread().queueUserApc([this, dimension]() {
        m_renderThread.windowRenderer().resize(dimension);
        m_renderThread.updated();
    });
    if (m_controller.state().duplicationStatus != DuplicationStatus::Live) {
        restart();
    }
}

void DuplicationController::updateOutputZoom(float zoom) {
    m_renderThread.thread().queueUserApc([this, zoom]() {
        m_renderThread.windowRenderer().zoomOutput(zoom);
        m_renderThread.updated();
    });
}

void DuplicationController::updateCaptureOffset(Vec2f offset) {
    m_renderThread.thread().queueUserApc([this, offset]() {
        m_renderThread.windowRenderer().updateOffset(offset);
        // m_renderThread.updated();
    });
    if (m_controller.state().duplicationStatus != DuplicationStatus::Live) {
        restart();
    }
}

void DuplicationController::restart() {
    stopOnMain();
    if (m_controller.state().duplicationStatus == DuplicationStatus::Live) {
        startOnMain();
    }
}

auto DuplicationController::renderThreadConfig(OperationModeLens const &lens) -> RenderThread::Config {
    auto config = RenderThread::Config{
        .pointerBuffer = m_pointerUpdater.data(),
        .outputZoom = lens.outputZoom(),
        .captureOffset = lens.captureOffset(),
    };
    config.setCallbacks(this);
    return config;
}

auto DuplicationController::captureThreadConfig() -> CaptureThread::Config {
    auto config = CaptureThread::Config{};
    config.threadIndex = 0;
    config.setCallbacks(this);
    return config;
}

// void DuplicationController::update() {
//     if (m_lastCaptureStatus != m_captureModel.status()) {
//         updateCaptureStatus();
//     }
//     if (m_isFreezed != m_duplicationModel.isFreezed()) {
//         m_isFreezed = m_duplicationModel.isFreezed();
//         if (!m_isFreezed) captureNextFrame();
//     }
//     if (m_lastCaptureStatus == CaptureStatus::Enabled) {
//         render();
//     }
// }

void DuplicationController::startOnMain() {
    if (m_status == Status::Paused) {
        m_renderWindow.show();
        m_captureThread.next();
        updateStatusOnMain(Status::Resuming);
        return;
    }
    auto canStart = [status = m_status.load()]() {
        using enum Status;
        switch (status) {
        case Stopped:
        case Paused:
        case Failed: return true;
        case Starting:
        case Live:
        case Stopping:
        case Resuming:
        case RetryStarting: return false;
        }
        return false;
    }();
    if (!canStart) return;
    try {
        try {
            auto deviceValue = renderer::createDevice();
            auto device = deviceValue.device;
            auto deviceContext = deviceValue.deviceContext;

            auto dimensionData = renderer::getDimensionData(device, {m_controller.operatonModeLens().captureMonitor()});
            m_controller.updateScreenRect(dimensionData.rect);
            m_displayRect = dimensionData.rect;

            m_targetTexture = renderer::createSharedTexture(device, dimensionData.rect.dimension);
            m_windowClass.recreateWindow(
                m_renderWindow, createWindowConfig(m_outputWindow, m_controller.config().outputDimension));
            m_renderWindow.setCustomHandler<&DuplicationController::forwardRenderMessage>(this);
            m_renderThread.thread().queueUserApc(
                [this,
                 initArgs = WindowRenderer::InitArgs{
                     .basic =
                         {
                             .device = device,
                             .deviceContext = deviceContext,
                         },
                     .windowHandle = m_renderWindow.handle(),
                     .windowDimension = m_controller.config().outputDimension,
                     .texture = m_targetTexture,
                 }]() mutable { m_renderThread.windowRenderer().init(std::move(initArgs)); });
            m_renderWindow.show();

            auto handle = renderer::getSharedHandle(m_targetTexture);
            updateStatusOnMain(Status::Starting);
            startCaptureThread(handle);
        }
        catch (const renderer::Error &e) {
            throw Expected{e.message};
        }
    }
    catch (const Expected &e) {
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
        resetOnMain();

        updateStatusOnMain(Status::RetryStarting);
        awaitRetry();
    }
}

void DuplicationController::pauseOnMain() {
    auto canPause = m_status == Status::Live;
    if (!canPause) return;

    updateStatusOnMain(Status::Paused);

    if (m_controller.config().operationMode == OperationMode::CaptureArea) {
        m_renderWindow.hide();
    }
}

void DuplicationController::stopOnMain() {
    updateStatusOnMain(Status::Stopping);
    resetOnMain();
    updateStatusOnMain(Status::Stopped);
}

void DuplicationController::resetOnMain() {
    try {
        m_captureThread.stop();
        // m_renderThread.stop();
        m_frameUpdater.reset();
        m_targetTexture.Reset();
        m_renderThread.reset();
    }
    catch (Expected &e) {
        OutputDebugStringA("resetOnMain Threw: ");
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
    }
}

void DuplicationController::updateStatusOnMain(Status status) {
    if (m_status.load() == status) return;
    m_status.store(status);
    m_controller.updateSystemStatus([status] {
        switch (status) {
        case Status::Stopped: return SystemStatus::Neutral;
        case Status::Live: return SystemStatus::Green;
        case Status::Starting:
        case Status::Stopping:
        case Status::Resuming:
        case Status::Paused: return SystemStatus::Yellow;
        case Status::RetryStarting:
        case Status::Failed: return SystemStatus::Red;
        }
        return SystemStatus::Red; // unknown
    }());
}

void DuplicationController::startCaptureThread(HANDLE targetHandle) {
    auto deviceValue = renderer::createDevice();
    auto device = deviceValue.device;
    auto deviceContext = deviceValue.deviceContext;

    auto updater_args = FrameUpdater::InitArgs{};
    updater_args.device = device;
    updater_args.deviceContext = deviceContext;
    updater_args.targetHandle = targetHandle;
    m_frameUpdater = FrameUpdater{std::move(updater_args)};

    auto threadArgs = CaptureThread::StartArgs{};
    threadArgs.display = m_controller.operatonModeLens().captureMonitor();
    threadArgs.device = device;
    threadArgs.offset = m_displayRect.topLeft;
    m_captureThread.start(std::move(threadArgs));
}

void DuplicationController::awaitRetry() {
    using namespace std::chrono_literals;
    auto timerArgs = WaitableTimer::SetArgs{};
    timerArgs.time = 250ms;
    auto success = m_retryTimer.set<&DuplicationController::retryTimeout>(timerArgs, {this});
    if (!success) throw Unexpected{"failed to arm retry timer"};
}

void DuplicationController::retryTimeout() { startOnMain(); }

auto DuplicationController::forwardRenderMessage(const win32::WindowMessage &msg) -> win32::OptLRESULT {
    return m_outputWindow.handleMessage(msg);
}

void DuplicationController::setError(std::exception_ptr error) {
    m_mainThread.queueUserApc([this, error]() {
        try {
            std::rethrow_exception(error);
        }
        catch (const Expected &e) {
            OutputDebugStringA(e.text);
            OutputDebugStringA("\n");
            stopOnMain();
            updateStatusOnMain(Status::Failed);
            awaitRetry();
        }
    });
}

void DuplicationController::setFrame(CapturedUpdate &&update, const FrameContext &context, size_t threadIndex) {
    m_renderThread.thread().queueUserApc([this, update = std::move(update), &context, threadIndex]() mutable {
        setFrameOnRender(std::move(update), context, threadIndex);
    });
}

void DuplicationController::setFrameOnRender(
    CapturedUpdate &&update, const FrameContext &context, size_t /*threadIndex*/) {

    // m_frameUpdaters[threadIndex].update(update.frame, context);
    m_frameUpdater->update(update.frame, context);
    m_pointerUpdater.update(update.pointer, context);
    m_renderThread.renderFrame();

    auto status = m_status.load();
    if (status == Status::Live) {
        m_captureThread.next();
    }
    else if (status == Status::Starting || status == Status::Resuming) {
        m_mainThread.queueUserApc([this]() {
            auto current = m_status.load();
            if (current == Status::Starting || current == Status::Resuming) {
                updateStatusOnMain(Status::Live);
                m_captureThread.next();
            }
        });
    }
}

} // namespace deskdup
