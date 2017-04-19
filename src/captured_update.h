#pragma once

#include "stable.h"

#include <meta/comptr.h>
#include <meta/array_view.h>

#include <vector>

using move_view = array_view<const DXGI_OUTDUPL_MOVE_RECT>;
using dirty_view = array_view<const RECT>;

struct captured_update {
	ComPtr<ID3D11Texture2D> desktop_image;

	DXGI_OUTDUPL_FRAME_INFO info;
	std::vector<uint8_t> meta_data;
	uint32_t move_bytes = 0;
	uint32_t dirty_bytes = 0;
	std::vector<uint8_t> pointer_data;
	DXGI_OUTDUPL_POINTER_SHAPE_INFO pointer_info;

	move_view moved() const {
		return move_view::from_bytes(meta_data.data(), move_bytes);
	}
	dirty_view dirty() const {
		return dirty_view::from_bytes(meta_data.data() + move_bytes, dirty_bytes);
	}
};