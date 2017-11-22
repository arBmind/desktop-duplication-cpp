#pragma once
#include "stable.h"

#include <meta/array_view.h>
#include <meta/comptr.h>

#include <vector>

using moved_view = array_view<const DXGI_OUTDUPL_MOVE_RECT>;
using dirty_view = array_view<const RECT>;

struct frame_update {
    int64_t present_time{};
    uint32_t frames{};
    bool rects_coalesced{};
    bool protected_content_masked_out{};

    ComPtr<ID3D11Texture2D> image; // texture with the entire display (in display device)

    std::vector<uint8_t> buffer; // managed buffer with move & dirty data
    uint32_t moved_bytes{};
    uint32_t dirty_bytes{};

    moved_view moved() const { return moved_view::from_bytes(buffer.data(), moved_bytes); }
    dirty_view dirty() const {
        return dirty_view::from_bytes(buffer.data() + moved_bytes, dirty_bytes);
    }
};

struct pointer_update {
    int64_t update_time{};
    DXGI_OUTDUPL_POINTER_POSITION position{};

    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info{}; // infos about pointer changes
    std::vector<uint8_t> shape_buffer; // managed buffer for new cursor shape
};

struct captured_update {
    frame_update frame;
    pointer_update pointer;
};
