
#include "application.h"
#include "capture_thread.h"
 
#include "meta/scope_guard.h"

constexpr const auto window_class_name = L"desdup";


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
	case WM_CLOSE: PostQuitMessage(0); break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void register_window_class(HINSTANCE instance) {
    auto cursor = LoadCursor(nullptr, IDC_CROSS);
	if (!cursor) throw Unexpected{ "LoadCursor failed" };
    LATER(DestroyCursor(cursor));

    WNDCLASSEXW window_class;
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = WndProc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.hIcon = nullptr;
    window_class.hCursor = cursor;
    window_class.hbrBackground = nullptr;
    window_class.lpszMenuName = nullptr;
    window_class.lpszClassName = window_class_name;
    window_class.hIconSm = nullptr;

    auto success = RegisterClassExW(&window_class);
	if (!success) throw Unexpected{ "Window class registration failed" };
}

HWND create_main_window(HINSTANCE instance, int nCmdShow) {
    auto rect = RECT{100, 100, 924, 678};
    auto style = WS_OVERLAPPEDWINDOW;
    auto hasMenu = false;
    AdjustWindowRect(&rect, style, hasMenu);

    static const auto title = L"Duplicate Desktop Presenter (Double Click to toggle fullscreen)";
    auto width = rect.right - rect.left;
    auto height = rect.bottom - rect.top;
    auto WindowHandle = CreateWindowW(window_class_name, title,
                           style,
                           rect.left, rect.top,
                           width, height,
                           nullptr, nullptr, instance, nullptr);
	if (!WindowHandle) throw Unexpected{ "Window creation failed" };

    ShowWindow(WindowHandle, nCmdShow);
    UpdateWindow(WindowHandle);

    return WindowHandle;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, char*, int nCmdShow) {
    try {
        register_window_class(hInstance);
        auto window = create_main_window(hInstance, nCmdShow);

		auto config = application::config{};
		config.displays.push_back(0);
		application app(window, std::move(config));
        return app.run();
    }
    catch (const Unexpected& e) {
		OutputDebugStringA(e.text);
		OutputDebugStringA("\n");
        return -1;
    }
}
