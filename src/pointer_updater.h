#pragma once
#include "stable.h"

#include <vector>

struct pointer_update;
struct frame_context;

struct pointer_buffer {
    int64_t position_timestamp = 0; // timestamp of the last postition & visible update
    POINT position{};
    bool visible = false;

    int64_t shape_timestamp = 0; // timestamp of last shape update
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info{};
    std::vector<uint8_t> shape_data; // buffer for with shape texture
};

struct pointer_updater {
    void update(pointer_update &update, const frame_context &context);

    const pointer_buffer &data() const { return pointer_m; }

private:
    RECT pointer_desktop_m = {0, 0, 0, 0};
    pointer_buffer pointer_m;
};
