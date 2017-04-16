#pragma once

#include "stable.h"

#include "captured_update.h"

#include <meta/comptr.h>
#include <array>

struct RenderFailure {
	RenderFailure(HRESULT res, const char* text) {}
};

struct frame_data {
	POINT offset;
	DXGI_OUTPUT_DESC output_desc;
};

struct frame_updater {
	struct setup {
		ComPtr<ID3D11Texture2D> target;
		ComPtr<ID3D11RenderTargetView> renderTarget;

		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> deviceContext;
	};

	void init(const setup& setup) {
		target_m = setup.target;
		renderTarget_m = setup.renderTarget;
		device_m = setup.device;
		deviceContext_m = setup.deviceContext;
	}
	void done() {
		moveTmp_m.Reset();
		renderTarget_m.Reset();
		target_m.Reset();
		deviceContext_m->ClearState();
		deviceContext_m->Flush();
		deviceContext_m.Reset();
		device_m.Reset();
	}

	void update(const captured_update& data, const frame_data& context);

private:
	void performMoves(const move_view& moves, const frame_data& context);
	void updateDirty(const ComPtr<ID3D11Texture2D> &desktop, const dirty_view& dirts, const frame_data& context);

private:
	ComPtr<ID3D11Texture2D> target_m;
	ComPtr<ID3D11RenderTargetView> renderTarget_m;

	ComPtr<ID3D11Device> device_m;
	ComPtr<ID3D11DeviceContext> deviceContext_m;


	ComPtr<ID3D11Texture2D> moveTmp_m;

	struct Vertex { float x, y, z, u, v; };
	using QuadVertices = std::array<Vertex, 6>;

	std::vector<QuadVertices> dirtyQuads_m;
};
