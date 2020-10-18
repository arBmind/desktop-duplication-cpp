#pragma once
#include "stable.h"

#include <vector>

struct PointerUpdate;
struct FrameContext;

struct PointerBuffer {
    int64_t position_timestamp = 0; // timestamp of the last postition & visible update
    POINT position{};
    bool visible = false;

    int64_t shape_timestamp = 0; // timestamp of last shape update
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info{};
    std::vector<uint8_t> shape_data; // buffer for with shape texture
};

struct PointerUpdater {
    void update(PointerUpdate &update, const FrameContext &context);

    const PointerBuffer &data() const noexcept { return pointer_m; }

private:
    RECT pointer_desktop_m = {0, 0, 0, 0};
    PointerBuffer pointer_m;
};
