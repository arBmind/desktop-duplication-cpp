#include "CaptureThread.h"

#include "application.h"
#include "renderer.h"

#include "CapturedUpdate.h"

#include "meta/scope_guard.h"
#include "meta/tuple.h"

#include <gsl.h>

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

auto CaptureThread::start(StartArgs &&args) -> std::thread {
    m_device = std::move(args.device);
    m_context.offset = args.offset;
    m_keepRunning = true;
    m_doCapture = true;
    return std::thread([=] { run(); });
}

void WINAPI CaptureThread::stopAPC(ULONG_PTR parameter) {
    auto args = unique_tuple_ptr_cast<CaptureThread *>(parameter);
    auto self = std::get<CaptureThread *>(*args);
    self->m_keepRunning = false;
}

void CaptureThread::nextAPC(ULONG_PTR parameter) {
    auto args = unique_tuple_ptr_cast<CaptureThread *>(parameter);
    auto self = std::get<CaptureThread *>(*args);
    self->m_dupl->ReleaseFrame();
    self->m_doCapture = true;
}

void CaptureThread::next() {
    [[gsl::suppress("26490")]] // bad API design
    QueueUserAPC(&CaptureThread::nextAPC, m_threadHandle, ulong_ptr_cast(make_tuple_ptr(this)));
}

void CaptureThread::stop() {
    [[gsl::suppress("26490")]] // bad API design
    QueueUserAPC(&CaptureThread::stopAPC, m_threadHandle, ulong_ptr_cast(make_tuple_ptr(this)));
}

void CaptureThread::run() {
    m_threadHandle = GetCurrentThreadHandle();
    LATER(CloseHandle(m_threadHandle));
    try {
        setDesktop();
        initDuplication();
        while (m_keepRunning) {
            if (m_doCapture) {
                auto frame = captureUpdate();
                if (frame) {
                    m_callbacks->setFrame(std::move(*frame), m_context, m_index);
                    m_doCapture = false;
                }
            }
            const auto timeout = m_doCapture ? 1 : INFINITE;
            const auto alertable = true;
            SleepEx(timeout, alertable);
        }
    }
    catch (...) {
        m_callbacks->setError(std::current_exception());
        while (m_keepRunning) {
            const auto alertable = true;
            SleepEx(INFINITE, alertable);
        }
    }
}

void CaptureThread::initDuplication() {
    ComPtr<IDXGIDevice> dxgi_device;
    auto result = m_device.As(&dxgi_device);
    if (IS_ERROR(result)) throw Unexpected{"Failed to get IDXGIDevice from device"};

    ComPtr<IDXGIAdapter> dxgi_adapter;
    result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
    const auto parentExpected = {DXGI_ERROR_ACCESS_LOST, HRESULT{WAIT_ABANDONED}};
    if (IS_ERROR(result))
        handleDeviceError("Failed to get IDXGIAdapter from device", result, parentExpected);

    ComPtr<IDXGIOutput> dxgi_output;
    result = dxgi_adapter->EnumOutputs(m_display, &dxgi_output);
    if (IS_ERROR(result))
        handleDeviceError("Failed to get ouput from adapter", result, {DXGI_ERROR_NOT_FOUND});

    result = dxgi_output->GetDesc(&m_context.output_desc);
    if (IS_ERROR(result)) handleDeviceError("Failed to get ouput description", result, {});

    ComPtr<IDXGIOutput1> dxgi_output1;
    result = dxgi_output.As(&dxgi_output1);
    if (IS_ERROR(result)) throw Unexpected{"Failed to get IDXGIOutput1 from dxgi_output"};

    result = dxgi_output1->DuplicateOutput(m_device.Get(), &m_dupl);
    if (IS_ERROR(result)) {
        if (DXGI_ERROR_NOT_CURRENTLY_AVAILABLE == result)
            throw Unexpected{"Maximum of desktop duplications reached!"};
        const auto duplicateExpected = {
            HRESULT{E_ACCESSDENIED},
            DXGI_ERROR_UNSUPPORTED,
            DXGI_ERROR_SESSION_DISCONNECTED,
        };
        handleDeviceError("Failed to get duplicate output from device", result, duplicateExpected);
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
    CapturedUpdate update;
    const auto time = 50;
    ComPtr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frame_info;
    auto result = m_dupl->AcquireNextFrame(time, &frame_info, &resource);
    if (DXGI_ERROR_WAIT_TIMEOUT == result) return {};
    if (IS_ERROR(result)) throw Expected{"Failed to acquire next frame in capture_thread"};

    update.frame.frames = frame_info.AccumulatedFrames;
    update.frame.present_time = frame_info.LastPresentTime.QuadPart;
    update.frame.rects_coalesced = frame_info.RectsCoalesced;
    update.frame.protected_content_masked_out = frame_info.ProtectedContentMaskedOut;

    if (0 != frame_info.TotalMetadataBufferSize) {
        update.frame.buffer.resize(frame_info.TotalMetadataBufferSize);

        auto moved_ptr = update.frame.buffer.data();
        result = m_dupl->GetFrameMoveRects(
            frame_info.TotalMetadataBufferSize,
            reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT *>(moved_ptr),
            &update.frame.moved_bytes);
        if (IS_ERROR(result)) throw Expected{"Failed to get frame moved rects in capture_thread"};

        auto dirty_ptr = moved_ptr + update.frame.moved_bytes;
        const auto dirty_size = frame_info.TotalMetadataBufferSize - update.frame.moved_bytes;
        result = m_dupl->GetFrameDirtyRects(
            dirty_size, reinterpret_cast<RECT *>(dirty_ptr), &update.frame.dirty_bytes);
        if (IS_ERROR(result)) throw Expected{"Failed to get frame dirty rects in capture_thread"};
    }
    if (!update.frame.dirty().empty()) {
        result = resource.As(&update.frame.image);
        if (IS_ERROR(result))
            throw Unexpected{"Failed to get ID3D11Texture from resource in capture_thread"};
    }

    update.pointer.update_time = frame_info.LastMouseUpdateTime.QuadPart;
    update.pointer.position = frame_info.PointerPosition;
    if (0 != frame_info.PointerShapeBufferSize) {
        update.pointer.shape_buffer.resize(frame_info.PointerShapeBufferSize);

        auto pointer_ptr = update.pointer.shape_buffer.data();
        const auto pointer_size = update.pointer.shape_buffer.size();
        UINT size_required_dummy;
        result = m_dupl->GetFramePointerShape(
            reinterpret_cast<const uint32_t &>(pointer_size),
            pointer_ptr,
            &size_required_dummy,
            &update.pointer.shape_info);
        if (IS_ERROR(result)) throw Expected{"Failed to get frame pointer shape in capture_thread"};
        // assert(size_required_dummy == frame.pointer_data.size());
    }
    return update;
}
