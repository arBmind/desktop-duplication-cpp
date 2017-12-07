#include "application.h"

int WINAPI
WinMain(_In_ HINSTANCE instanceHandle, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int showCommand) {
    auto config = application::config{};
    config.instanceHandle = instanceHandle;
    config.showCommand = showCommand;
    config.displays.push_back(0); // TODO: parse commandline / config file

    const auto hr = CoInitialize(NULL);
    if (!SUCCEEDED(hr)) return -1;

    application app(std::move(config));
    return app.run();
}
