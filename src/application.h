#pragma once
#include "stable.h"

#include <memory> // unique_ptr
#include <vector>

struct Application {
    struct Config {
        HINSTANCE instanceHandle;
        bool showCommand;

        std::vector<int> displays;
        float zoom = 1;
        // RECT rect;
    };

    Application(Config);

    int run();

private:
    struct Impl;
    struct ImplDeleter {
        void operator()(Impl *) noexcept;
    };
    using ImplPtr = std::unique_ptr<Impl, ImplDeleter>;
    ImplPtr m;
};
