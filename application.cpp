#include "application.h"

#include "capture_thread.h"
#include "renderer.h"
#include "frame_updater.h"

#include "meta/tuple.h"
#include "meta/scope_guard.h"

struct internal : capture_thread::api {
	using config = application::config;

	internal(HWND window, config && cfg)
		: window_m(window), config_m(cfg) {

		int index = 0;
		for (auto display : config_m.displays) {
			capture_threads_m.emplace_back(new capture_thread(display, this, index++));
		}
		threads_m.reserve(config_m.displays.size());
	}

	static void WINAPI setErrorAPC(ULONG_PTR parameter) {
		auto args = unique_tuple_ptr<internal*, std::exception_ptr>(parameter);
		std::get<internal*>(*args)->updateError(std::get<1>(*args));
	}
	static void WINAPI setFrameAPC(ULONG_PTR parameter) {
		auto args = unique_tuple_ptr<internal*, captured_update, std::reference_wrapper<frame_data>, int>(parameter);
		std::get<internal*>(*args)->updateFrame(std::get<1>(*args), std::get<2>(*args), std::get<3>(*args));
	}

	int run() {
		threadHandle_m = GetCurrentThreadHandle();
		LATER(CloseHandle(threadHandle_m));
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
			auto deviceValue = renderer::createDevice();
			auto device = deviceValue.device;
			auto deviceContext = deviceValue.deviceContext;

			auto factory = renderer::getFactory(device);

			auto swapChain = renderer::createSwapChain(factory, device, window_m);

			renderer::device_args args;
			args.device = device;
			args.deviceContext = deviceContext;
			args.swapChain = swapChain;
			renderDevice_m = renderer::device::create(args);

			auto dimensions = renderer::getDesktopData(device, config_m.displays);

			target_m = renderer::createTexture(device, dimensions.desktop_rect);

			frame_updater::setup setup;
			setup.device = device;
			setup.deviceContext = deviceContext;
			setup.target = target_m;
			setup.renderTarget = renderer::renderToTexture(device, target_m);
			frame_updater_m.init(setup);

			auto offset = POINT{ dimensions.desktop_rect.left, dimensions.desktop_rect.top };
			for (auto& capture : capture_threads_m) {
				threads_m.emplace_back(capture->start(device, offset));
			}
		}
		catch (const renderer::error& e) {
			throw Expected{ e.message };
		}
	}

	void stopDuplication() {
		if (!duplicationStarted_m) return;
		try {
			for (auto& capture : capture_threads_m) {
				capture->stop();
			}
			for (auto& thread : threads_m) {
				thread.join();
			}
			threads_m.clear();
			target_m.Reset();
			frame_updater_m.done();
			renderDevice_m.reset();
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
		try {
			renderDevice_m.render(target_m);
			renderDevice_m.swap();
			auto clone = updated_threads_m;
			updated_threads_m.clear();
			for (auto index : clone) capture_threads_m[index]->next();
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
		auto wakeMask = QS_ALLINPUT;
		auto flags = MWMO_ALERTABLE | MWMO_INPUTAVAILABLE;
		auto awoken = MsgWaitForMultipleObjectsEx(
			(uint32_t)handles_m.size(), handles_m.data(),
			timeout, wakeMask, flags);
		if (WAIT_FAILED == awoken)
			throw Unexpected{ "Application Waiting failed" };
		if (WAIT_OBJECT_0 == awoken - handles_m.size())
			processMessages();
	}

	void processMessages() {
		auto msg = MSG{};
		auto hasMsg = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
		if (!hasMsg) return;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (WM_QUIT == msg.message) {
			keepRunning_m = false;
			returnValue_m = (int)msg.wParam;
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
		frame_updater_m.update(frame, context);
		updated_threads_m.push_back(thread_index);
		doRender_m = true;
	}

private:
	HWND window_m;
	config config_m;
public:
	HANDLE threadHandle_m;
private:
	bool duplicationStarted_m = false;
	bool keepRunning_m = true;
	int returnValue_m = 0;
	bool hasError_m = false;
	std::exception_ptr error_m;
	bool doRender_m = false;

	renderer::device renderDevice_m;
	ComPtr<ID3D11Texture2D> target_m;
	frame_updater frame_updater_m;

	using capture_thread_ptr = std::unique_ptr<capture_thread>;

	std::vector<HANDLE> handles_m;
	std::vector<std::thread> threads_m;
	std::vector<capture_thread_ptr> capture_threads_m;
	std::vector<int> updated_threads_m;
};

struct application::internal: ::internal {
	using ::internal::internal;
};

application::application(HWND window, config&& cfg)
	: ip(internal_ptr{ new internal(window, std::move(cfg)) }) {}

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
