#pragma once
#include "stable.h"

#include "base_renderer.h"

#include <meta/comptr.h>
#include <array>
#include <vector>

struct frame_update;
struct frame_context;

struct RenderFailure {
	RenderFailure(HRESULT res, const char* text) {}
};

struct frame_updater {
	struct init_args : base_renderer::init_args {
		HANDLE targetHandle; // shared texture handle that should be updated
	};
	using vertex = base_renderer::vertex;
	using quad_vertices = std::array<vertex, 6>;

	frame_updater(init_args&& args);

	void update(const frame_update& data, const frame_context& context);

private:
	void performMoves(const frame_update &data, const frame_context& context);
	void updateDirty(const frame_update &data, const frame_context& context);

private:
	struct resources : base_renderer {
		resources(frame_updater::init_args&& args);

		void prepare(HANDLE targetHandle);

		void activateRenderTarget() {
			deviceContext()->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);
		}

		ComPtr<ID3D11Texture2D> target_m;
		ComPtr<ID3D11RenderTargetView> renderTarget_m;

		ComPtr<ID3D11Texture2D> moveTmp_m;
	};
	resources dx_m;

	std::vector<quad_vertices> dirtyQuads_m;
};
