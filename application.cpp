#include "application.h"

#include "capture_thread.h"
#include "renderer.h"
#include "frame_updater.h"
#include "window_renderer.h"

#include "meta/tuple.h"
#include "meta/scope_guard.h"

#include <windowsx.h>

namespace {
	constexpr const auto WINDOM_CLASS_NAME = L"desdup";

	bool IsWindowMaximized(HWND windowHandle) {
		WINDOWPLACEMENT placement{ sizeof(WINDOWPLACEMENT) };
		auto success = GetWindowPlacement(windowHandle, &placement);
		return success && placement.showCmd == SW_MAXIMIZE;
	}

	void ShowWindowBorder(HWND windowHandle, bool shown) {
		auto style = GetWindowLong(windowHandle, GWL_STYLE);
		const auto flags = WS_BORDER | WS_SIZEBOX | WS_DLGFRAME;
		if (shown) {
			style |= flags;
		}
		else {
			style &= ~flags;
		}

		SetWindowLong(windowHandle, GWL_STYLE, style);
	}
}

struct internal: capture_thread::api{
	using config = application::config;

	internal(config&& cfg) : config_m(std::move(cfg)) {}

	static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
		static internal* self = nullptr;
		static auto armed = false;
		static auto dragging = false;
		static LPARAM lastpos;
		switch (message) {
		case WM_CREATE:
			if (nullptr == self) {
				auto create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
				self = reinterpret_cast<internal*>(create_struct->lpCreateParams);
			}
			break;
		//case WM_DESTROY: //???
		case WM_CLOSE:
			PostQuitMessage(0);
			break;
		case WM_LBUTTONDBLCLK:
			self->toggleMaximized();
			break;
		case WM_SIZE:
			self->handleSizeChanged();
			break;

		// Zoom
		case WM_MOUSEWHEEL:
			if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)) {
				auto wheel_delta = float(GET_WHEEL_DELTA_WPARAM(wParam))/(30*WHEEL_DELTA);
				if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT)) {
					wheel_delta /= 10.f;
				}
				self->changeZoom(wheel_delta);
			}
			break;

		// Offset Dragging
		case WM_LBUTTONDOWN:
			if (GET_KEYSTATE_WPARAM(wParam) == (MK_LBUTTON | MK_SHIFT)) {
				dragging = true;
				lastpos = lParam;
			}
			break;
		case WM_LBUTTONUP:
			dragging = false;
			break;
		case WM_MOUSEMOVE:
			if (dragging && GET_KEYSTATE_WPARAM(wParam) == (MK_LBUTTON | MK_SHIFT)) {
				auto delta = POINT{
					GET_X_LPARAM(lParam) - GET_X_LPARAM(lastpos),
					GET_Y_LPARAM(lParam) - GET_Y_LPARAM(lastpos) };
				self->moveTexture(delta);
				lastpos = lParam;
			}
			break;

		// Fit Other Window
		case WM_RBUTTONDBLCLK:
			armed ^= true;
			break;
		case WM_KILLFOCUS:
			if (armed) {
				armed = false;
				auto other_window = GetForegroundWindow();
				self->fitWindow(other_window);
			}
			break;

		default:
			return DefWindowProc(window, message, wParam, lParam);
		}
		return 0;
	}

	void handleSizeChanged() {
		if (!duplicationStarted_m) return;
		RECT rect;
		GetClientRect(windowHandle_m, &rect);
		windowRenderer_m.resize(rectSize(rect));
		doRender_m = true;
	}

	void changeZoom(float zoomDelta) {
		if (!duplicationStarted_m) return;
		auto zoom = windowRenderer_m.zoom() + zoomDelta;
		windowRenderer_m.setZoom(std::max(zoom, 0.05f));
		doRender_m = true;
	}

	void toggleMaximized() {
		::ShowWindow(windowHandle_m, IsWindowMaximized(windowHandle_m) ? SW_SHOWNORMAL : SW_MAXIMIZE);
	}

	void moveTexture(POINT delta) {
		windowRenderer_m.moveOffset(delta);
		doRender_m = true;
	}

	void fitWindow(HWND otherWindow) {
		RECT window_rect;
		GetClientRect(windowHandle_m, &window_rect);
		auto window_size = rectSize(window_rect);
		RECT other_rect;
		GetWindowRect(otherWindow, &other_rect);
		auto zoom = windowRenderer_m.zoom();
		auto renderOffset = windowRenderer_m.offset();
		auto left = offset_m.x - renderOffset.x;
		auto top = offset_m.y - renderOffset.y;
		auto width = long(window_size.cx / zoom);
		auto height = long(window_size.cy / zoom);
		auto repaint = true;
		MoveWindow(otherWindow, left, top, width, height, repaint);
	}

	static void registerWindowClass(HINSTANCE instanceHandle) {
		auto cursor = LoadCursor(nullptr, IDC_CROSS);
		if (!cursor) throw Unexpected{ "LoadCursor failed" };
		LATER(DestroyCursor(cursor));

		WNDCLASSEXW window_class;
		window_class.cbSize = sizeof(WNDCLASSEXW);
		window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		window_class.lpfnWndProc = windowProc;
		window_class.cbClsExtra = 0;
		window_class.cbWndExtra = 0;
		window_class.hInstance = instanceHandle;
		window_class.hIcon = nullptr;
		window_class.hCursor = cursor;
		window_class.hbrBackground = nullptr;
		window_class.lpszMenuName = nullptr;
		window_class.lpszClassName = WINDOM_CLASS_NAME;
		window_class.hIconSm = nullptr;

		auto success = RegisterClassExW(&window_class);
		if (!success) throw Unexpected{ "Window class registration failed" };
	}

	HWND createMainWindow(HINSTANCE instanceHandle, int showCommand) {
		auto rect = RECT{ 100, 100, 924, 678 };
		auto style = WS_OVERLAPPEDWINDOW;
		auto has_menu = false;
		AdjustWindowRect(&rect, style, has_menu);

		static const auto title = L"Duplicate Desktop Presenter (Double Click to toggle fullscreen)";
		auto width = rect.right - rect.left;
		auto height = rect.bottom - rect.top;
		auto parent_window = nullptr;
		auto menu = nullptr;
		auto custom_param = this;
		auto window_handle = CreateWindowW(WINDOM_CLASS_NAME, title,
			style,
			rect.left, rect.top,
			width, height,
			parent_window, menu, instanceHandle, custom_param);
		if (!window_handle) throw Unexpected{ "Window creation failed" };

		ShowWindow(window_handle, showCommand);
		UpdateWindow(window_handle);

		return window_handle;
	}

	static void WINAPI setErrorAPC(ULONG_PTR parameter) {
		auto args = unique_tuple_ptr<internal*, std::exception_ptr>(parameter);
		std::get<internal*>(*args)->updateError(std::get<1>(*args));
	}
	static void WINAPI setFrameAPC(ULONG_PTR parameter) {
		auto args = unique_tuple_ptr<internal*, captured_update, std::reference_wrapper<frame_data>, int>(parameter);
		std::get<internal*>(*args)->updateFrame(std::get<1>(*args), std::get<2>(*args), std::get<3>(*args));
	}

	void initCaptureThreads() {
		auto threadIndex = 0;
		for (auto display : config_m.displays) {
			captureThreads_m.emplace_back(new capture_thread(display, this, threadIndex++));
		}
		threads_m.reserve(config_m.displays.size());
	}

	int run() {
		try {
			registerWindowClass(config_m.instanceHandle);
			threadHandle_m = GetCurrentThreadHandle();
			LATER(CloseHandle(threadHandle_m));

			windowHandle_m = createMainWindow(config_m.instanceHandle, config_m.showCommand);
			initCaptureThreads();

			return mainLoop();
		}
		catch (const Unexpected& e) {
			OutputDebugStringA(e.text);
			OutputDebugStringA("\n");
			return -1;
		}
	}

	int mainLoop() {
		while (keepRunning_m) {
			try {
				startDuplication();
			}
			catch (Expected& e) {
				OutputDebugStringA(e.text);
				OutputDebugStringA("\n");
				stopDuplication();
				awaitRetry();
				continue;
			}
			try {
				while (keepRunning_m && duplicationStarted_m && !hasError_m) {
					render();
					sleep();
				}
				rethrow();
			}
			catch (Expected& e) {
				OutputDebugStringA(e.text);
				OutputDebugStringA("\n");
				error_m = {};
				stopDuplication();
			}
		}
		stopDuplication();
		return returnValue_m;
	}

	void startDuplication() {
		if (duplicationStarted_m) return;
		duplicationStarted_m = true;
		try {
			auto device_value = renderer::createDevice();
			auto device = device_value.device;
			auto device_context = device_value.deviceContext;

			auto dimensions = renderer::getDesktopData(device, config_m.displays);

			target_m = renderer::createSharedTexture(device, rectSize(dimensions.desktop_rect));

			window_renderer::init_args window_args;
			window_args.windowHandle = windowHandle_m;
			window_args.texture = target_m;
			window_args.device = device;
			window_args.deviceContext = device_context;
			windowRenderer_m.init(std::move(window_args));

			offset_m = { dimensions.desktop_rect.left, dimensions.desktop_rect.top };
			auto handle = renderer::getSharedHandle(target_m);
			for (auto& capture : captureThreads_m) {
				startCaptureThread(*capture, offset_m, handle);
			}
		}
		catch (const renderer::error& e) {
			throw Expected{ e.message };
		}
	}

	void startCaptureThread(capture_thread& capture, const POINT& offset, HANDLE targetHandle) {
		auto device_value = renderer::createDevice();
		auto device = device_value.device;
		auto device_context = device_value.deviceContext;

		frame_updater::init_args updater_args;
		updater_args.device = device;
		updater_args.deviceContext = device_context;
		updater_args.targetHandle = targetHandle;
		frameUpdaters_m.emplace_back(std::move(updater_args));

		capture_thread::start_args args;
		args.device = device;
		args.offset = offset;
		threads_m.emplace_back(capture.start(std::move(args)));
	}

	void stopDuplication() {
		if (!duplicationStarted_m) return;
		try {
			for (auto& capture : captureThreads_m) {
				capture->stop();
			}
			for (auto& thread : threads_m) {
				thread.join();
			}
			threads_m.clear();
			target_m.Reset();
			frameUpdaters_m.clear();
			windowRenderer_m.reset();
		}
		catch (Expected& e) {
			OutputDebugStringA("stopDuplication Threw: ");
			OutputDebugStringA(e.text);
			OutputDebugStringA("\n");
		}
		duplicationStarted_m = false;
	}

	void render() {
		if (!doRender_m) return;
		doRender_m = false;
		ShowWindowBorder(windowHandle_m, !IsWindowMaximized(windowHandle_m));
		try {
			windowRenderer_m.render();
			windowRenderer_m.swap();
			auto clone = updatedThreads_m;
			updatedThreads_m.clear();
			for (auto index : clone) captureThreads_m[index]->next();
		}
		catch (const renderer::error& e) {
			throw Expected{ e.message };
		}
	}

	void awaitRetry() {
		auto timeout = 250;
		sleep(timeout);
	}

	void sleep(int timeout = INFINITE) {
		auto wake_mask = QS_ALLINPUT;
		auto flags = MWMO_ALERTABLE | MWMO_INPUTAVAILABLE;
		auto awoken = MsgWaitForMultipleObjectsEx(
			(uint32_t)handles_m.size(), handles_m.data(),
			timeout, wake_mask, flags);
		if (WAIT_FAILED == awoken)
			throw Unexpected{ "Application Waiting failed" };
		if (WAIT_OBJECT_0 == awoken - handles_m.size())
			processMessages();
	}

	void processMessages() {
		while (true) {
			auto message = MSG{};
			auto success = PeekMessage(&message, nullptr, 0, 0, PM_REMOVE);
			if (!success) return;
			TranslateMessage(&message);
			DispatchMessage(&message);
			if (WM_QUIT == message.message) {
				keepRunning_m = false;
				returnValue_m = (int)message.wParam;
			}
		}
	}

	void rethrow() {
		if (!hasError_m) return;
		hasError_m = false;
		std::rethrow_exception(error_m);
	}

	void updateError(std::exception_ptr& exception) {
		error_m = exception;
		hasError_m = true;
	}
	void updateFrame(captured_update& frame, const frame_data& context, int thread_index) {
		frameUpdaters_m[thread_index].update(frame, context);
		updatedThreads_m.push_back(thread_index);
		doRender_m = true;
	}

private:
	config config_m;
	HWND windowHandle_m;
public:
	HANDLE threadHandle_m;
private:
	bool duplicationStarted_m = false;
	bool keepRunning_m = true;
	int returnValue_m = 0;
	bool hasError_m = false;
	std::exception_ptr error_m;
	bool doRender_m = false;

	POINT offset_m;
	window_renderer windowRenderer_m;
	ComPtr<ID3D11Texture2D> target_m;
	std::vector<frame_updater> frameUpdaters_m;

	using capture_thread_ptr = std::unique_ptr<capture_thread>;

	std::vector<HANDLE> handles_m;
	std::vector<std::thread> threads_m;
	std::vector<capture_thread_ptr> captureThreads_m;
	std::vector<int> updatedThreads_m;
};

struct application::internal: ::internal {
	using ::internal::internal;
};

application::application(config&& cfg)
	: ip(internal_ptr{ new internal(std::move(cfg)) }) {}

int application::run() {
	return ip->run();
}

void capture_thread::api::setError(std::exception_ptr error) {
	auto self = reinterpret_cast<internal*>(this);
	auto parameter = make_tuple_ptr(self, error);
	auto success = QueueUserAPC(internal::setErrorAPC, self->threadHandle_m, (ULONG_PTR)parameter);
	if (!success) throw Unexpected{ "api::setError failed to queue APC" };
}

void capture_thread::api::setFrame(captured_update&& frame, const frame_data& context, int thread_index) {
	auto self = reinterpret_cast<internal*>(this);
	auto parameter = make_tuple_ptr(self, (captured_update&&)frame, std::ref(context), thread_index);
	auto success = QueueUserAPC(internal::setFrameAPC, self->threadHandle_m, (ULONG_PTR)parameter);
	if (!success) throw Unexpected{ "api::setError failed to queue APC" };
}

void application::internal_deleter::operator()(internal *ptr) {
	delete ptr;
}
