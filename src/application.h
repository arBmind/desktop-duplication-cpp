#pragma once
#include "stable.h"

#include <memory>
#include <vector>

struct application {
    struct config {
        HINSTANCE instanceHandle;
        bool showCommand;

        std::vector<int> displays;
        float zoom = 1;
        // RECT rect;
    };

    application(config &&cfg);

    int run();

private:
    struct internal;
    struct internal_deleter {
        void operator()(internal *) noexcept;
    };
    std::unique_ptr<internal, internal_deleter> ip;
};
