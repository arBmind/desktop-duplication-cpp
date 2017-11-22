#include "capture_thread.h"

#include "application.h"
#include "renderer.h"

#include "captured_update.h"

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

HANDLE GetCurrentThreadHandle() {
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

std::thread capture_thread::start(start_args &&args) {
    device_m = std::move(args.device);
    context_m.offset = args.offset;
    keepRunning_m = true;
    doCapture_m = true;
    return std::thread([=] { run(); });
}

void WINAPI capture_thread::stopAPC(ULONG_PTR parameter) {
    auto args = unique_tuple_ptr<capture_thread *>(parameter);
    auto self = std::get<capture_thread *>(*args);
    self->keepRunning_m = false;
}

void capture_thread::nextAPC(ULONG_PTR parameter) {
    auto args = unique_tuple_ptr<capture_thread *>(parameter);
    auto self = std::get<capture_thread *>(*args);
    self->dupl_m->ReleaseFrame();
    self->doCapture_m = true;
}

void capture_thread::next() {
    auto parameter = make_tuple_ptr(this);
    QueueUserAPC(&capture_thread::nextAPC, threadHandle_m, reinterpret_cast<ULONG_PTR>(parameter));
}

void capture_thread::stop() {
    auto parameter = make_tuple_ptr(this);
    QueueUserAPC(&capture_thread::stopAPC, threadHandle_m, reinterpret_cast<ULONG_PTR>(parameter));
}

void capture_thread::run() {
    threadHandle_m = GetCurrentThreadHandle();
    LATER(CloseHandle(threadHandle_m));
    const auto alertable = true;
    try {
        setDesktop();
        initDuplication();
        while (keepRunning_m) {
            if (doCapture_m) {
                auto frame = captureUpdate();
                if (frame) {
                    callbacks_m->setFrame(std::move(*frame), context_m, index_m);
                    doCapture_m = false;
                }
            }
            const auto timeout = doCapture_m ? 1 : INFINITE;
            SleepEx(timeout, alertable);
        }
    }
    catch (...) {
        callbacks_m->setError(std::current_exception());
        while (keepRunning_m) {
            SleepEx(INFINITE, alertable);
        }
    }
}

void capture_thread::initDuplication() {
    ComPtr<IDXGIDevice> dxgi_device;
    auto result = device_m.As(&dxgi_device);
    if (IS_ERROR(result)) throw Unexpected{"Failed to get IDXGIDevice from device"};

    ComPtr<IDXGIAdapter> dxgi_adapter;
    result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
    auto parentExpected = {DXGI_ERROR_ACCESS_LOST, static_cast<HRESULT>(WAIT_ABANDONED)};
    if (IS_ERROR(result))
        handleDeviceError("Failed to get IDXGIAdapter from device", result, parentExpected);

    ComPtr<IDXGIOutput> dxgi_output;
    result = dxgi_adapter->EnumOutputs(display_m, &dxgi_output);
    if (IS_ERROR(result))
        handleDeviceError("Failed to get ouput from adapter", result, {DXGI_ERROR_NOT_FOUND});

    result = dxgi_output->GetDesc(&context_m.output_desc);
    if (IS_ERROR(result)) handleDeviceError("Failed to get ouput description", result, {});

    ComPtr<IDXGIOutput1> dxgi_output1;
    result = dxgi_output.As(&dxgi_output1);
    if (IS_ERROR(result)) throw Unexpected{"Failed to get IDXGIOutput1 from dxgi_output"};

    result = dxgi_output1->DuplicateOutput(device_m.Get(), &dupl_m);
    if (IS_ERROR(result)) {
        if (DXGI_ERROR_NOT_CURRENTLY_AVAILABLE == result)
            throw Unexpected{"Maximum of desktop duplications reached!"};
        auto duplicateExpected = {
            static_cast<HRESULT>(E_ACCESSDENIED),
            DXGI_ERROR_UNSUPPORTED,
            DXGI_ERROR_SESSION_DISCONNECTED,
        };
        handleDeviceError("Failed to get duplicate output from device", result, duplicateExpected);
    }
}

void capture_thread::handleDeviceError(
    const char *text, HRESULT result, std::initializer_list<HRESULT> expected) {
    if (device_m) {
        const auto reason = device_m->GetDeviceRemovedReason();
        if (S_OK != reason) throw Expected{text};
    }
    for (const auto cand : expected) {
        if (result == cand) throw Expected{text};
    }
    throw Unexpected{text};
}

std::optional<captured_update> capture_thread::captureUpdate() {
    captured_update update;
    auto time = 50;
    ComPtr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frame_info;
    auto result = dupl_m->AcquireNextFrame(time, &frame_info, &resource);
    if (DXGI_ERROR_WAIT_TIMEOUT == result) return {};
    if (IS_ERROR(result)) throw Expected{"Failed to acquire next frame in capture_thread"};

    update.frame.frames = frame_info.AccumulatedFrames;
    update.frame.present_time = frame_info.LastPresentTime.QuadPart;
    update.frame.rects_coalesced = frame_info.RectsCoalesced;
    update.frame.protected_content_masked_out = frame_info.ProtectedContentMaskedOut;

    if (0 != frame_info.TotalMetadataBufferSize) {
        update.frame.buffer.resize(frame_info.TotalMetadataBufferSize);

        auto moved_ptr = update.frame.buffer.data();
        result = dupl_m->GetFrameMoveRects(
            static_cast<uint32_t>(update.frame.buffer.size()),
            reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT *>(moved_ptr),
            &update.frame.moved_bytes);
        if (IS_ERROR(result)) throw Expected{"Failed to get frame moved rects in capture_thread"};

        auto dirty_ptr = moved_ptr + update.frame.moved_bytes;
        result = dupl_m->GetFrameDirtyRects(
            static_cast<uint32_t>(update.frame.buffer.size()),
            reinterpret_cast<RECT *>(dirty_ptr),
            &update.frame.dirty_bytes);
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
        UINT size_required_dummy;
        result = dupl_m->GetFramePointerShape(
            static_cast<uint32_t>(update.pointer.shape_buffer.size()),
            pointer_ptr,
            &size_required_dummy,
            &update.pointer.shape_info);
        if (IS_ERROR(result)) throw Expected{"Failed to get frame pointer shape in capture_thread"};
        // assert(size_required_dummy == frame.pointer_data.size());
    }
    return update;
}
