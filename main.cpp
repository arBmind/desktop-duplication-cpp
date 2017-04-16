#include "application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, char*, int nCmdShow) {
	auto config = application::config{};
	config.instanceHandle = hInstance;
	config.showCommand = nCmdShow;
	config.displays.push_back(0); // TODO: parse commandline / config file

	application app(std::move(config));
    return app.run();
}
