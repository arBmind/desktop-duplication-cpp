#include "application.h"

auto WINAPI WinMain(
    _In_ HINSTANCE instanceHandle, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int showCommand) -> int {
    auto make_config = [&] {
        auto config = Application::Config{};
        config.instanceHandle = instanceHandle;
        config.showCommand = showCommand;
        config.displays.push_back(0); // TODO: parse commandline / config file
        return config;
    };

    const auto hr = CoInitialize(nullptr);
    if (!SUCCEEDED(hr)) return -1;

    auto app = Application{make_config()};
    return app.run();
}
