#include "CaptureThread.h"

#include "renderer.h"

#include "CapturedUpdate.h"

#include "meta/scope_guard.h"

namespace {

void setDesktop() {
    const auto flags = 0;
    const auto inherit = false;
    const auto access = GENERIC_ALL;
    auto desktop = OpenInputDesktop(flags, inherit, access);
    if (!desktop) throw Expected{"Failed to open desktop"};
    LATER(CloseDesktop(desktop));

    const auto result = SetThreadDesktop(desktop);
    if (!result) throw Expected{"Failed to set desktop"};
}

} // namespace

auto GetCurrentThreadHandle() -> HANDLE {
    HANDLE output{};
    const auto process = GetCurrentProcess();
    const auto thread = GetCurrentThread();
    const auto desiredAccess = 0;
    const auto inheritHandle = false;
    const auto options = DUPLICATE_SAME_ACCESS;
    const auto success =
        DuplicateHandle(process, thread, process, &output, desiredAccess, inheritHandle, options);
    if (!success) throw Unexpected{"could not get thread handle"};
    return output;
}

CaptureThread::~CaptureThread() {
    if (m_stdThread) stop();
}

void CaptureThread::start(StartArgs &&args) {
    if (m_stdThread) return; // already started
    m_device = std::move(args.device);
    m_context.offset = args.offset;
    m_keepRunning = true;
    m_doCapture = true;
    m_stdThread.emplace([=] { run(); });
}

void CaptureThread::next() { m_thread.queueUserApc([this](){ capture_next(); }); }

void CaptureThread::stop() {
    m_thread.queueUserApc([this](){ capture_stop(); });

    m_stdThread->join();
    m_stdThread.reset();
}

void CaptureThread::capture_next() {
    m_dupl->ReleaseFrame();
    m_doCapture = true;
}

void CaptureThread::capture_stop() { m_keepRunning = false; }

void CaptureThread::run() {
    m_thread = Thread::fromCurrent();
    try {
        setDesktop();
        initCapture();
        while (m_keepRunning) {
            if (m_doCapture) {
                auto frame = captureUpdate();
                if (frame) {
                    m_config.setFrameCallback(
                        m_config.callbackPtr, std::move(*frame), m_context, m_config.threadIndex);
                    m_doCapture = false;
                }
            }
            const auto timeout = m_doCapture ? 1 : INFINITE;
            const auto alertable = true;
            SleepEx(timeout, alertable);
        }
    }
    catch (...) {
        m_config.setErrorCallback(m_config.callbackPtr, std::current_exception());
        while (m_keepRunning) {
            const auto alertable = true;
            SleepEx(INFINITE, alertable);
        }
    }
}

void CaptureThread::initCapture() {
    auto dxgiDevice = ComPtr<IDXGIDevice>{};
    auto dxResult = m_device.As(&dxgiDevice);
    if (IS_ERROR(dxResult)) throw Unexpected{"Failed to get IDXGIDevice from device"};

    auto dxgiAdapter = ComPtr<IDXGIAdapter>{};
    dxResult = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), &dxgiAdapter);
    const auto parentExpected = {DXGI_ERROR_ACCESS_LOST, HRESULT{WAIT_ABANDONED}};
    if (IS_ERROR(dxResult))
        handleDeviceError("Failed to get IDXGIAdapter from device", dxResult, parentExpected);

    auto dxgiOutput = ComPtr<IDXGIOutput>{};
    dxResult = dxgiAdapter->EnumOutputs(m_config.display, &dxgiOutput);
    if (IS_ERROR(dxResult))
        handleDeviceError("Failed to get ouput from adapter", dxResult, {DXGI_ERROR_NOT_FOUND});

    dxResult = dxgiOutput->GetDesc(&m_context.output_desc);
    if (IS_ERROR(dxResult)) handleDeviceError("Failed to get ouput description", dxResult, {});

    auto dxgiOutput1 = ComPtr<IDXGIOutput1>{};
    dxResult = dxgiOutput.As(&dxgiOutput1);
    if (IS_ERROR(dxResult)) throw Unexpected{"Failed to get IDXGIOutput1 from dxgi_output"};

    dxResult = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_dupl);
    if (IS_ERROR(dxResult)) {
        if (DXGI_ERROR_NOT_CURRENTLY_AVAILABLE == dxResult)
            throw Unexpected{"Maximum of desktop duplications reached!"};
        const auto duplicateExpected = {
            HRESULT{E_ACCESSDENIED},
            DXGI_ERROR_UNSUPPORTED,
            DXGI_ERROR_SESSION_DISCONNECTED,
        };
        handleDeviceError(
            "Failed to get duplicate output from device", dxResult, duplicateExpected);
    }
}

void CaptureThread::handleDeviceError(
    const char *text, HRESULT result, std::initializer_list<HRESULT> expected) {
    if (m_device) {
        const auto reason = m_device->GetDeviceRemovedReason();
        if (S_OK != reason) throw Expected{text};
    }
    for (const auto cand : expected) {
        if (result == cand) throw Expected{text};
    }
    throw Unexpected{text};
}

auto CaptureThread::captureUpdate() -> std::optional<CapturedUpdate> {
    auto result = std::optional<CapturedUpdate>{};
    const auto time = 10;
    auto resource = ComPtr<IDXGIResource>{};
    auto frameInfo = DXGI_OUTDUPL_FRAME_INFO{};
    auto dxResult = m_dupl->AcquireNextFrame(time, &frameInfo, &resource);
    if (DXGI_ERROR_WAIT_TIMEOUT == dxResult) return {};
    if (IS_ERROR(dxResult)) throw Expected{"Failed to acquire next frame in capture_thread"};

    result.emplace();
    auto &update = result.value();
    update.frame.frames = frameInfo.AccumulatedFrames;
    update.frame.present_time = frameInfo.LastPresentTime.QuadPart;
    update.frame.rects_coalesced = frameInfo.RectsCoalesced;
    update.frame.protected_content_masked_out = frameInfo.ProtectedContentMaskedOut;

    if (0 != frameInfo.TotalMetadataBufferSize) {
        update.frame.buffer.resize(frameInfo.TotalMetadataBufferSize);

        auto movedPtr = update.frame.buffer.data();
        dxResult = m_dupl->GetFrameMoveRects(
            frameInfo.TotalMetadataBufferSize,
            reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT *>(movedPtr),
            &update.frame.moved_bytes);
        if (IS_ERROR(dxResult)) throw Expected{"Failed to get frame moved rects in capture_thread"};

        auto dirtyPtr = movedPtr + update.frame.moved_bytes;
        const auto dirtySize = frameInfo.TotalMetadataBufferSize - update.frame.moved_bytes;
        dxResult = m_dupl->GetFrameDirtyRects(
            dirtySize, reinterpret_cast<RECT *>(dirtyPtr), &update.frame.dirty_bytes);
        if (IS_ERROR(dxResult)) throw Expected{"Failed to get frame dirty rects in capture_thread"};
    }
    if (!update.frame.dirty().empty()) {
        dxResult = resource.As(&update.frame.image);
        if (IS_ERROR(dxResult))
            throw Unexpected{"Failed to get ID3D11Texture from resource in capture_thread"};
    }

    update.pointer.update_time = frameInfo.LastMouseUpdateTime.QuadPart;
    update.pointer.position = frameInfo.PointerPosition;
    if (0 != frameInfo.PointerShapeBufferSize) {
        update.pointer.shape_buffer.resize(frameInfo.PointerShapeBufferSize);

        auto pointerPtr = update.pointer.shape_buffer.data();
        const auto pointerSize = update.pointer.shape_buffer.size();
        auto sizeRequiredDummy = UINT{};
        dxResult = m_dupl->GetFramePointerShape(
            reinterpret_cast<const uint32_t &>(pointerSize),
            pointerPtr,
            &sizeRequiredDummy,
            &update.pointer.shape_info);
        if (IS_ERROR(dxResult))
            throw Expected{"Failed to get frame pointer shape in capture_thread"};
        // assert(size_required_dummy == frame.pointer_data.size());
    }
    return update;
}
