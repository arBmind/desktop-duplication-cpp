#pragma once

#include "stable.h"

#include "captured_update.h"
#include "frame_updater.h"

#include <meta/comptr.h>
#include <meta/tuple.h>

#include <vector>
#include <optional>
#include <thread>


struct Unexpected {
	const char* text;
};

struct Expected {
	const char* text;
};

HANDLE GetCurrentThreadHandle();

struct capture_thread {
	struct api {
		// callbacks are implemented in application.cpp
		void setError(std::exception_ptr error);
		void setFrame(captured_update&& frame, const frame_data& context, int thread_index);
	};

	capture_thread(int display, api* api, int index)
		: api_m(api)
		, display_m(display)
		, index_m(index) {}

	std::thread start(ComPtr<ID3D11Device> device, POINT offset);

	void next();
	void stop();

private:
	static void WINAPI stopAPC(ULONG_PTR);
	static void WINAPI nextAPC(ULONG_PTR);

	void run();

	void initDupl();
	void handleDeviceError(const char* text, HRESULT result, std::initializer_list<HRESULT> expected);
	std::optional<captured_update> captureFrame();

private:
	int display_m;
	int index_m;
	api* api_m;
	frame_data context_m;
	ComPtr<ID3D11Device> device_m;
	HANDLE threadHandle_m;
	bool keepRunning_m = true;
	bool doCapture_m = true;

	ComPtr<IDXGIOutputDuplication> dupl_m;
};
