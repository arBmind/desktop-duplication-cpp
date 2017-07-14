#include "application.h"

int WINAPI WinMain(HINSTANCE instanceHandle, HINSTANCE, char*, int showCommand) {
	auto config = application::config{};
	config.instanceHandle = instanceHandle;
	config.showCommand = showCommand;
	config.displays.push_back(0); // TODO: parse commandline / config file

   config.showCaptureFrameRate = wcsstr( GetCommandLine(), L"--ShowCaptureFPS" ) != nullptr;

	application app(std::move(config));
    return app.run();
}
