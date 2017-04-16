#include "capture_thread.h"

#include "application.h"

#include "meta/scope_guard.h"

namespace {

	void setDesktop() {
		auto flags = 0;
		auto inherit = false;
		auto access = GENERIC_ALL;
		auto desktop = OpenInputDesktop(flags, inherit, access);
		if (!desktop) throw Expected{ "Failed to open desktop" };
		LATER(CloseDesktop(desktop));

		auto result = SetThreadDesktop(desktop);
		if (!result) throw Expected { "Failed to set desktop" };
	}

} // namespace

HANDLE GetCurrentThreadHandle() {
	HANDLE output;
	auto inheritHandle = false;
	auto success = DuplicateHandle(
		GetCurrentProcess(), GetCurrentThread(),
		GetCurrentProcess(), &output,
		0, inheritHandle, DUPLICATE_SAME_ACCESS);
	if (!success) throw Unexpected{ "could not get thread handle" };
	return output;
}

std::thread capture_thread::start(ComPtr<ID3D11Device> device, POINT offset) {
	device_m = device;
	context_m.offset = offset;
	keepRunning_m = true;
	doCapture_m = true;
	return std::thread([=] { run(); });
}

void WINAPI capture_thread::stopAPC(ULONG_PTR parameter) {
	auto args = unique_tuple_ptr<capture_thread*>(parameter);
	auto self = std::get<capture_thread*>(*args);
	self->keepRunning_m = false;
}

void capture_thread::nextAPC(ULONG_PTR parameter) {
	auto args = unique_tuple_ptr<capture_thread*>(parameter);
	auto self = std::get<capture_thread*>(*args);
	self->dupl_m->ReleaseFrame();
	self->doCapture_m = true;
}

void capture_thread::next() {
	auto parameter = make_tuple_ptr(this);
	QueueUserAPC(&capture_thread::nextAPC, threadHandle_m, (ULONG_PTR)parameter);
}

void capture_thread::stop() {
	auto parameter = make_tuple_ptr(this);
	QueueUserAPC(&capture_thread::stopAPC, threadHandle_m, (ULONG_PTR)parameter);
}

void capture_thread::run() {
	threadHandle_m = GetCurrentThreadHandle();
	LATER(CloseHandle(threadHandle_m));
	const auto alertable = true;
	try {
		setDesktop();
		initDupl();
		while (keepRunning_m) {
			if (doCapture_m) {
				auto frame = captureFrame();
				if (frame) {
					api_m->setFrame((captured_update&&)*frame, context_m, index_m);
					doCapture_m = false;
				}
			}
			auto timeout = doCapture_m ? 10 : INFINITE;
			SleepEx(timeout, alertable);
		}
	}
	catch (...) {
		api_m->setError(std::current_exception());
		while (keepRunning_m) {
			SleepEx(INFINITE, alertable);
		}
	}
}

void capture_thread::initDupl() {
	ComPtr<IDXGIDevice> dxgi_device;
	auto result = device_m.As(&dxgi_device);
	if (IS_ERROR(result)) throw Unexpected{ "Failed to get IDXGIDevice from device" };

	ComPtr<IDXGIAdapter> dxgi_adapter;
	result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
	auto parentExpected = {
		DXGI_ERROR_ACCESS_LOST,
		static_cast<HRESULT>(WAIT_ABANDONED)
	};
	if (IS_ERROR(result)) handleDeviceError("Failed to get IDXGIAdapter from device", result, parentExpected);

	ComPtr<IDXGIOutput> dxgi_output;
	result = dxgi_adapter->EnumOutputs(display_m, &dxgi_output);
	if (IS_ERROR(result)) handleDeviceError("Failed to get ouput from adapter", result, { DXGI_ERROR_NOT_FOUND });

	result = dxgi_output->GetDesc(&context_m.output_desc);
	if (IS_ERROR(result)) handleDeviceError("Failed to get ouput description", result, {});

	ComPtr<IDXGIOutput1> dxgi_output1;
	result = dxgi_output.As(&dxgi_output1);
	if (IS_ERROR(result)) throw Unexpected{ "Failed to get IDXGIOutput1 from dxgi_output" };

	result = dxgi_output1->DuplicateOutput(device_m.Get(), &dupl_m);
	if (IS_ERROR(result)) {
		if (DXGI_ERROR_NOT_CURRENTLY_AVAILABLE == result)
			throw Unexpected{ "Maximum of desktop duplications reached!" };
		auto duplicateExpected = {
			static_cast<HRESULT>(E_ACCESSDENIED),
			DXGI_ERROR_UNSUPPORTED,
			DXGI_ERROR_SESSION_DISCONNECTED,
		};
		handleDeviceError("Failed to get duplicate output from device", result, duplicateExpected);
	}
}

void capture_thread::handleDeviceError(const char* text, HRESULT result, std::initializer_list<HRESULT> expected) {
	if (device_m) {
		auto reason = device_m->GetDeviceRemovedReason();
		if (S_OK != reason) throw Expected{ text };
	}
	for (auto cand : expected) {
		if (result == cand) throw Expected{ text };
	}
	throw Unexpected{ text };
}

std::optional<captured_update> capture_thread::captureFrame() {
	captured_update frame;
	auto time = 50;
	ComPtr<IDXGIResource> resource;
	auto result = dupl_m->AcquireNextFrame(time, &frame.info, &resource);
	if (DXGI_ERROR_WAIT_TIMEOUT == result) return {};
	if (IS_ERROR(result)) throw Expected{ "Failed to acquire next frame in capture_thread" };

	result = resource.As(&frame.desktop_image);
	if (IS_ERROR(result)) throw Unexpected{ "Failed to get ID3D11Texture2D from resource in capture_thread" };

	if (0 != frame.info.TotalMetadataBufferSize) {
		frame.meta_data.resize(frame.info.TotalMetadataBufferSize);

		auto moved_ptr = frame.meta_data.data();
		result = dupl_m->GetFrameMoveRects(
			(uint32_t)frame.meta_data.size(),
			reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(moved_ptr),
			&frame.move_bytes);
		if (IS_ERROR(result)) throw Expected{ "Failed to get frame moved rects in capture_thread" };

		auto dirty_ptr = moved_ptr + frame.move_bytes;
		result = dupl_m->GetFrameDirtyRects(
			(uint32_t)frame.meta_data.size(),
			reinterpret_cast<RECT*>(dirty_ptr),
			&frame.dirty_bytes);
		if (IS_ERROR(result)) throw Expected{ "Failed to get frame dirty rects in capture_thread" };
	}
	if (0 != frame.info.PointerShapeBufferSize) {
		frame.pointer_data.resize(frame.info.PointerShapeBufferSize);

		auto pointer_ptr = frame.pointer_data.data();
		result = dupl_m->GetFramePointerShape(
			(uint32_t)frame.pointer_data.size(),
			pointer_ptr, &frame.pointer_bytes, 
			&frame.pointer_shape);
		if (IS_ERROR(result)) throw Expected{ "Failed to get frame pointer shape in capture_thread" };
	}
	return frame;
}
