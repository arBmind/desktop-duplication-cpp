#pragma once
#include <dxgi1_3.h>

#include <stdint.h>
#include <vector>

struct PointerUpdate;
struct FrameContext;

struct PointerBuffer {
    uint64_t position_timestamp = 0; // timestamp of the last postition & visible update
    POINT position{};
    bool visible = false;

    uint64_t shape_timestamp = 0; // timestamp of last shape update
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info{};
    std::vector<uint8_t> shape_data{}; // buffer for with shape texture
};

struct PointerUpdater {
    void update(PointerUpdate &update, const FrameContext &context);

    auto data() const noexcept -> const PointerBuffer & { return m_pointer; }

private:
    RECT m_pointer_desktop = {0, 0, 0, 0};
    PointerBuffer m_pointer;
};
