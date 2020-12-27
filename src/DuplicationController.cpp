#include "DuplicationController.h"
#include "CapturedUpdate.h"
#include "FrameUpdater.h"
#include "renderer.h"

#include "meta/tuple.h"

#include <chrono>

namespace deskdup {
namespace {

auto createRetryTimer() -> WaitableTimer {
    auto config = WaitableTimer::Config{};
    config.timerName = TEXT("Local:RetryTimer");
    return WaitableTimer{config};
}

} // namespace

DuplicationController::DuplicationController(Model &model, ThreadLoop &threadLoop, Window &window)
    : m_captureModel(model.capture())
    , m_duplicationModel(model.duplication())
    , m_threadLoop(threadLoop)
    , m_outputWindow(window)
    , m_mainThread(Thread::fromCurrent())
    , m_retryTimer(createRetryTimer()) {}

void DuplicationController::update() {
    if (m_lastCaptureStatus != m_captureModel.status()) {
        updateCaptureStatus();
    }
    if (m_isFreezed != m_duplicationModel.isFreezed()) {
        m_isFreezed = m_duplicationModel.isFreezed();
        if (!m_isFreezed) captureNextFrame();
    }
    if (m_lastCaptureStatus == CaptureStatus::Enabled) {
        render();
    }
}

void DuplicationController::initCaptureThread() {
    auto config = CaptureThread::Config{};
    config.display = m_captureModel.monitor();
    config.threadIndex = 0;
    config.setCallbacks(this);
    m_captureThread.emplace(config);
}

void DuplicationController::updateCaptureStatus() {
    if (m_lastCaptureStatus == CaptureStatus::Disabled) {
        start();
    }
    else if (m_lastCaptureStatus == CaptureStatus::Enabled) {
        if (m_captureModel.status() == CaptureStatus::Disabled) {
            stop();
        }
    }
}

void DuplicationController::start() {
    if (m_captureModel.status() != CaptureStatus::Enabling) return;
    if (!m_captureThread) initCaptureThread();
    try {
        try {
            auto deviceValue = renderer::createDevice();
            auto device = deviceValue.device;
            auto deviceContext = deviceValue.deviceContext;

            auto dimensionData = renderer::getDimensionData(device, {m_captureModel.monitor()});
            m_captureModel.setVirtualRect(dimensionData.rect);

            m_targetTexture = renderer::createSharedTexture(device, dimensionData.rect.dimension);
            m_windowRenderer.init([&]() noexcept {
                auto args = WindowRenderer::InitArgs{};
                args.windowHandle = m_outputWindow.handle();
                args.texture = m_targetTexture;
                args.device = device;
                args.deviceContext = deviceContext;
                return args;
            }());
            auto handle = renderer::getSharedHandle(m_targetTexture);
            stopNextFrame(); // capturing starts anyways
            startCaptureThread(handle);
        }
        catch (const renderer::Error &e) {
            throw Expected{e.message};
        }

        m_captureModel.setStatus(CaptureStatus::Enabled);
        m_lastCaptureStatus = CaptureStatus::Enabled;
    }
    catch (const Expected &e) {
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
        stop();
        m_captureModel.setStatus(CaptureStatus::Error);
        awaitRetry();
    }
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
    threadArgs.device = device;
    threadArgs.offset = m_captureModel.virtualRect().topLeft;
    m_captureThread->start(std::move(threadArgs));
}

void DuplicationController::stop() {
    try {
        m_captureThread->stop();
        m_targetTexture.Reset();
        m_frameUpdater = {};
        m_hasFrameUpdated = false; // m_updatedThreads.clear();
        m_windowRenderer.reset();
        stopNextFrame();

        m_captureModel.setStatus(CaptureStatus::Disabled);
        m_lastCaptureStatus = CaptureStatus::Disabled;
    }
    catch (Expected &e) {
        OutputDebugStringA("stopDuplication Threw: ");
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
    }
}

void DuplicationController::awaitRetry() {
    using namespace std::chrono_literals;
    auto timerArgs = WaitableTimer::SetArgs{};
    timerArgs.time = 250ms;
    auto success = m_retryTimer.set<&DuplicationController::retryTimeout>(timerArgs, {this});
    if (!success) throw Unexpected{"failed to arm retry timer"};
}

void DuplicationController::render() {
    try {
        rethrow();
        if (m_waitFrame || (!m_hasFrameUpdated && !m_hasLastUpdate)) return;
        m_hasLastUpdate = m_hasFrameUpdated;
        m_hasFrameUpdated = false;
        try {
            // captureNextFrame();
            m_windowRenderer.resize(m_duplicationModel.outputRect().dimension);
            m_windowRenderer.setZoom(m_duplicationModel.zoom());
            m_windowRenderer.setOffset(m_duplicationModel.captureOffset());
            m_windowRenderer.render();
            m_windowRenderer.renderPointer(m_pointerUpdater.data());
            m_windowRenderer.swap();
            awaitNextFrame();
        }
        catch (const renderer::Error &e) {
            throw Expected{e.message};
        }
    }
    catch (const Expected &e) {
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
        stop();
        m_captureModel.setStatus(CaptureStatus::Error);
        m_hasError = false;
        m_error = {};
    }
}

void DuplicationController::captureNextFrame() {
    if (m_isFreezed) return;
    // for (auto index : m_updatedThreads) m_captureThreads[index]->next();
    // m_updatedThreads.clear();
    if (m_triggerCaptureThread) {
        m_captureThread->next();
        m_triggerCaptureThread = false;
    }
}

void DuplicationController::awaitNextFrame() {
    if (!m_waitFrame) {
        m_waitFrame = true;

        if (!m_waitFrameHandle) {
            m_waitFrameHandle = m_windowRenderer.frameLatencyWaitable();
            m_threadLoop.addAwaitableMember<&DuplicationController::nextFrame>(
                m_waitFrameHandle.get(), this);
        }
    }
}

void DuplicationController::stopNextFrame() {
    if (m_waitFrameHandle) {
        m_threadLoop.removeAwaitable(m_waitFrameHandle.get());
        m_waitFrameHandle.reset();
        m_waitFrame = false;
    }
}

auto DuplicationController::nextFrame(HANDLE) -> ThreadLoop::Keep {
    m_waitFrame = false;
    return ThreadLoop::Keep::Yes;
}

void DuplicationController::setError(std::exception_ptr error) {
    m_mainThread.queueUserApc<&DuplicationController::main_setError>({this, error});
}

void DuplicationController::main_setError(std::exception_ptr exception) {
    m_error = exception;
    m_hasError = true;
}

void DuplicationController::rethrow() {
    if (!m_hasError) return;
    m_hasError = false;
    std::rethrow_exception(m_error);
}

void DuplicationController::setFrame(
    CapturedUpdate &&update, const FrameContext &context, size_t threadIndex) {

    m_mainThread.queueUserApc<&DuplicationController::main_setFrame>(
        {this, std::move(update), std::ref(context), threadIndex});
}

void DuplicationController::main_setFrame(
    CapturedUpdate &update, const FrameContext &context, size_t /*threadIndex*/) {

    if (m_lastCaptureStatus != CaptureStatus::Enabled) return;
    // m_frameUpdaters[threadIndex].update(update.frame, context);
    m_frameUpdater->update(update.frame, context);
    m_pointerUpdater.update(update.pointer, context);
    m_hasFrameUpdated = true;
    m_triggerCaptureThread = true;
    captureNextFrame();
}

void DuplicationController::retryTimeout() { m_captureModel.setStatus(CaptureStatus::Enabling); }

} // namespace deskdup
