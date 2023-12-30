#pragma once
#include "meta/fromByteSpan.h"
#include "meta/comptr.h"

#include <d3d11.h>
#include <dxgi1_3.h>

#include <vector>

using moved_view = std::span<const DXGI_OUTDUPL_MOVE_RECT>;
using dirty_view = std::span<const RECT>;

struct FrameUpdate {
    int64_t present_time{};
    uint32_t frames{};
    bool rects_coalesced{};
    bool protected_content_masked_out{};

    ComPtr<ID3D11Texture2D> image{}; // texture with the entire display (in display device)

    std::vector<uint8_t> buffer{}; // managed buffer with move & dirty data
    uint32_t moved_bytes{};
    uint32_t dirty_bytes{};

    auto moved() const noexcept -> moved_view {
        auto moved_span = std::span<const uint8_t>{buffer.data(), moved_bytes};
        return fromByteSpan<moved_view>(moved_span);
    }
    auto dirty() const noexcept -> dirty_view {
#pragma warning(suppress : 26481)
        auto dirty_span = std::span<const uint8_t>{buffer.data() + moved_bytes, dirty_bytes};
        return fromByteSpan<dirty_view>(dirty_span);
    }
};

struct PointerUpdate {
    uint64_t update_time{};
    DXGI_OUTDUPL_POINTER_POSITION position{};

    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info{}; // infos about pointer changes
    std::vector<uint8_t> shape_buffer{}; // managed buffer for new cursor shape
};

struct CapturedUpdate {
    FrameUpdate frame{};
    PointerUpdate pointer{};
};
