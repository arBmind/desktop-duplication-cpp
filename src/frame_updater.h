#pragma once

#include "base_renderer.h"

#include "captured_update.h"
#include "frame_context.h"

#include <meta/comptr.h>
#include <array>

struct RenderFailure {
	RenderFailure(HRESULT res, const char* text) {}
};

struct frame_updater : base_renderer {
	struct init_args : base_renderer::init_args {
		HANDLE targetHandle;
	};

	frame_updater(init_args&& args);

	void reset() {
		base_renderer::reset();
		moveTmp_m.Reset();
		renderTarget_m.Reset();
		target_m.Reset();
	}

	void update(const captured_update& data, const frame_data& context);

private:
	void init(init_args&& args);
	void prepare(HANDLE targetHandle);
	void performMoves(const move_view& moves, const frame_data& context);
	void updateDirty(const captured_update &data, const dirty_view& dirts, const frame_data& context);

private:
	ComPtr<ID3D11Texture2D> target_m;
	ComPtr<ID3D11RenderTargetView> renderTarget_m;

	ComPtr<ID3D11Texture2D> moveTmp_m;
	std::vector<QuadVertices> dirtyQuads_m;
};
